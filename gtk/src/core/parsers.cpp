#include "core/parsers.h"

#include <glib.h>
#include <glibmm/datetime.h>
#include <json-glib/json-glib.h>

#include <algorithm>
#include <filesystem>
#include <regex>
#include <set>

namespace dnfhelper::parsers {

namespace {

bool startsWith(const std::string &text, const std::string &prefix)
{
    return text.rfind(prefix, 0) == 0;
}

bool endsWith(const std::string &text, const std::string &suffix)
{
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

std::string lowercase(const std::string &text)
{
    gchar *lower = g_ascii_strdown(text.c_str(), -1);
    std::string result(lower);
    g_free(lower);
    return result;
}

std::vector<std::string> split(const std::string &text, const char separator)
{
    std::vector<std::string> parts;
    std::string::size_type start = 0;
    while (true) {
        const auto end = text.find(separator, start);
        parts.push_back(text.substr(start, end - start));
        if (end == std::string::npos)
            return parts;
        start = end + 1;
    }
}

std::int64_t toInt64(const std::string &text)
{
    try {
        std::size_t used = 0;
        const long long value = std::stoll(text, &used);
        return used == text.size() ? value : 0;
    } catch (const std::exception &) {
        return 0;
    }
}

std::string fileName(const std::string &path)
{
    return std::filesystem::path(path).filename().string();
}

bool isDirectory(const std::string &path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

} // namespace

std::string packageQueryFormat()
{
    return "%{name}|%{epoch}|%{version}|%{release}|%{arch}|%{repoid}|%{from_repo}|%{reason}|"
           "%{installsize}|%{installtime}|%{summary}|%{license}|%{url}|%{sourcerpm}|%{vendor}|"
           "%{packager}\\n";
}

std::string trimmed(const std::string &text)
{
    const char *whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

std::vector<std::string> nonEmptyLines(const std::string &text)
{
    std::vector<std::string> lines;
    for (const std::string &line : split(text, '\n')) {
        std::string clean = trimmed(line);
        if (!clean.empty())
            lines.push_back(std::move(clean));
    }
    return lines;
}

Package packageFromDelimitedLine(const std::string &line)
{
    const std::vector<std::string> parts = split(line, '|');
    Package package;
    if (parts.size() < 16)
        return package;

    // rpm prints "(none)" for an unset tag, such as the epoch of most packages.
    const auto value = [&parts](const std::size_t index) {
        std::string field = trimmed(parts[index]);
        return field == "(none)" ? std::string() : field;
    };

    package.id.name = value(0);
    package.id.epoch = value(1);
    package.id.version = value(2);
    package.id.release = value(3);
    package.id.arch = value(4);
    const std::string repoid = value(5);
    const std::string fromRepo = value(6);

    // repoid is the actual install-source repo in DNF5 (may be "@System" on older versions).
    // from_repo is whichever enabled repo currently provides this in metadata; it is not the
    // original install source. Prefer repoid when it is meaningful.
    package.repoId = !repoid.empty() && repoid != "@System" ? repoid : fromRepo;

    package.installReason = value(7);
    package.installSize = toInt64(value(8));
    package.installTime = std::max<std::int64_t>(0, toInt64(value(9)));
    package.summary = value(10);
    package.license = value(11);
    package.url = value(12);
    package.sourceRpm = value(13);
    package.vendor = value(14);
    package.packager = value(15);
    // repoid == @System is the DNF4-style installed marker; a non-empty from_repo means the
    // package was installed from somewhere. Neither: an --available result.
    package.installed = repoid == "@System" || !fromRepo.empty();
    return package;
}

std::vector<Package> packagesFromDelimitedOutput(const std::string &text)
{
    std::vector<Package> packages;
    for (const std::string &line : nonEmptyLines(text)) {
        Package package = packageFromDelimitedLine(line);
        if (package.id.isValid())
            packages.push_back(std::move(package));
    }
    return packages;
}

std::vector<DependencyEdge> dependencyEdgesFromLines(const std::string &text,
                                                     const DependencyType type,
                                                     const std::string &packageName)
{
    std::vector<DependencyEdge> edges;
    std::set<std::string> seen;
    for (const std::string &line : nonEmptyLines(text)) {
        if (!seen.insert(line).second)
            continue;
        DependencyEdge edge;
        edge.from.name = packageName;
        edge.type = type;
        edge.capability = line;
        edges.push_back(std::move(edge));
    }
    return edges;
}

Rows historyRowsFromOutput(const std::string &text, const std::string &packageName)
{
    static const std::regex transaction(
        R"(^\s*(\d+)\s+(.+?)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})\s+(.*?)\s+(\d+)\s*$)");
    Rows rows;
    for (const std::string &line : nonEmptyLines(text)) {
        if (startsWith(line, "ID ") || startsWith(lowercase(line), "no transaction"))
            continue;
        std::smatch match;
        if (std::regex_match(line, match, transaction)) {
            const std::string action = trimmed(match[4].str());
            rows.push_back({match[1].str(), match[3].str(), action.empty() ? "Changed" : action,
                            packageName, trimmed(match[2].str())});
        } else {
            rows.push_back({"—", "—", "Transaction", packageName, line});
        }
    }
    if (rows.empty())
        rows.push_back({"—", "—", "No matching history", packageName,
                        "DNF history did not report a transaction for this package."});
    return rows;
}

namespace {

// Parses JSON with json-glib and returns the root array, or nullptr. The parser owns the
// array; release it with g_object_unref().
JsonArray *jsonArray(const std::string &text, JsonParser *parser)
{
    if (!json_parser_load_from_data(parser, text.c_str(), static_cast<gssize>(text.size()),
                                    nullptr))
        return nullptr;
    JsonNode *root = json_parser_get_root(parser);
    return root && JSON_NODE_HOLDS_ARRAY(root) ? json_node_get_array(root) : nullptr;
}

std::string stringMember(JsonObject *object, const char *name)
{
    JsonNode *node = json_object_get_member(object, name);
    if (!node || !JSON_NODE_HOLDS_VALUE(node))
        return {};
    if (json_node_get_value_type(node) == G_TYPE_STRING)
        return json_node_get_string(node);
    return std::to_string(json_node_get_int(node));
}

// "kineticwe-0:0.0.1-19.fc44.x86_64" → "0.0.1-19.fc44".
std::string evrFromNevra(const std::string &nevra, const std::string &name)
{
    std::string evr = nevra.size() > name.size() + 1 ? nevra.substr(name.size() + 1) : "";
    const auto arch = evr.rfind('.');
    if (arch != std::string::npos)
        evr.erase(arch);
    if (evr.rfind("0:", 0) == 0)
        evr.erase(0, 2);
    return evr;
}

} // namespace

std::optional<std::vector<std::string>> historyIdsFromJson(const std::string &text)
{
    JsonParser *parser = json_parser_new();
    std::optional<std::vector<std::string>> ids;
    if (JsonArray *array = jsonArray(text, parser)) {
        ids.emplace();
        for (guint i = 0; i < json_array_get_length(array); ++i) {
            JsonNode *node = json_array_get_element(array, i);
            if (!JSON_NODE_HOLDS_OBJECT(node))
                continue;
            const std::string id = stringMember(json_node_get_object(node), "id");
            if (!id.empty())
                ids->push_back(id);
        }
    }
    g_object_unref(parser);
    return ids;
}

std::optional<Rows> historyRowsFromInfoJson(const std::string &text,
                                            const std::string &packageName)
{
    JsonParser *parser = json_parser_new();
    JsonArray *array = jsonArray(text, parser);
    if (!array) {
        g_object_unref(parser);
        return std::nullopt;
    }
    Rows rows;
    for (guint i = 0; i < json_array_get_length(array); ++i) {
        JsonNode *node = json_array_get_element(array, i);
        if (!JSON_NODE_HOLDS_OBJECT(node))
            continue;
        JsonObject *transaction = json_node_get_object(node);
        std::string date = "—";
        if (const std::int64_t start = toInt64(stringMember(transaction, "start_time")); start > 0)
            date = Glib::DateTime::create_now_local(start).format("%F %T");

        // The package's own entries: the new build, and the build it replaced.
        std::string action;
        std::string version;
        std::string replaced;
        if (JsonNode *packages = json_object_get_member(transaction, "packages");
            packages && JSON_NODE_HOLDS_ARRAY(packages)) {
            JsonArray *list = json_node_get_array(packages);
            for (guint j = 0; j < json_array_get_length(list); ++j) {
                JsonNode *entry = json_array_get_element(list, j);
                if (!JSON_NODE_HOLDS_OBJECT(entry))
                    continue;
                JsonObject *package = json_node_get_object(entry);
                const std::string nevra = stringMember(package, "nevra");
                if (packageNameFromNevra(nevra) != packageName)
                    continue;
                const std::string entryAction = stringMember(package, "action");
                if (entryAction == "Replaced") {
                    replaced = evrFromNevra(nevra, packageName);
                } else if (action.empty()) {
                    action = entryAction;
                    version = evrFromNevra(nevra, packageName);
                }
            }
        }
        if (action.empty() && !replaced.empty()) {
            action = "Replaced";
            version = replaced;
        } else if (!replaced.empty() && replaced != version) {
            version = replaced + " → " + version;
        }
        rows.push_back({stringMember(transaction, "id"), date,
                        action.empty() ? "Changed" : action, version,
                        stringMember(transaction, "description")});
    }
    g_object_unref(parser);
    if (rows.empty())
        rows.push_back({"—", "—", "No matching history", "",
                        "DNF history did not report a transaction for this package."});
    return rows;
}

std::string descriptionFromOutput(const std::string &text)
{
    std::string description;
    for (const std::string &line : nonEmptyLines(text)) {
        if (line == "Updating and loading repositories:" || line == "Repositories loaded.")
            continue;
        if (!description.empty())
            description += '\n';
        description += line;
    }
    return trimmed(description);
}

std::string packageNameFromNevra(const std::string &text)
{
    static const std::regex epoch(R"(-\d+:)");
    static const std::regex version(R"(-\d)");
    const std::string clean = trimmed(text);
    std::smatch match;
    if (std::regex_search(clean, match, epoch) && match.position(0) > 0)
        return clean.substr(0, static_cast<std::size_t>(match.position(0)));
    if (std::regex_search(clean, match, version) && match.position(0) > 0)
        return clean.substr(0, static_cast<std::size_t>(match.position(0)));
    return clean;
}

bool isNevra(const std::string &text)
{
    return packageNameFromNevra(text) != trimmed(text);
}

std::string sourceNameFromSourceRpm(const std::string &sourceRpm)
{
    std::string text = trimmed(sourceRpm);
    if (text.empty() || text == "(none)")
        return {};
    if (endsWith(text, ".rpm"))
        text.erase(text.size() - 4);
    if (endsWith(text, ".src") || endsWith(text, ".nosrc"))
        text.erase(text.rfind('.'));
    return packageNameFromNevra(text);
}

std::string capabilityQuery(const std::string &capability)
{
    std::string text = trimmed(capability);
    // A rich dependency: look up its first package, as in "(foo = 1 if bar)".
    if (!text.empty() && text.front() == '(')
        text = trimmed(text.substr(1));
    const auto space = text.find(' ');
    if (space != std::string::npos)
        text.erase(space);
    return text;
}

std::string repoKind(const std::string &repoId)
{
    const std::string lower = lowercase(repoId);
    if (lower == "fedora" || lower == "updates" || startsWith(lower, "fedora-")
        || startsWith(lower, "updates-"))
        return "Fedora repository";
    if (contains(lower, "copr"))
        return "COPR repository";
    if (contains(lower, "rpmfusion"))
        return "RPM Fusion repository";
    if (lower == "@system")
        return "Installed system package";
    if (lower.empty())
        return "Unknown";
    return "Third-party repository";
}

std::string packageNameFromDependencyCapability(const std::string &capability)
{
    static const std::regex plausible(R"(^[A-Za-z0-9_+.-]+$)");
    std::string text = trimmed(capability);
    const auto space = text.find(' ');
    if (space != std::string::npos && space > 0)
        text.erase(space);
    if (startsWith(text, "/") || contains(text, ".so") || contains(lowercase(text), "rtld")
        || contains(text, "GNU_HASH") || contains(text, "GLIBC") || contains(text, "CXXABI")
        || contains(text, "GLIBCXX"))
        return {};
    // "libdnf5(x86-64)" names a package for one architecture. Any other "name(...)" is a
    // namespaced capability, not a package: rpmlib(PayloadIsZstd), config(foot), perl(Foo).
    static const std::regex archQualified(R"(^([A-Za-z0-9_+.-]+)\([a-z0-9_]+-(32|64)\)$)");
    std::smatch match;
    if (std::regex_match(text, match, archQualified))
        return match[1].str();
    if (contains(text, "(") || contains(text, ")"))
        return {};
    return std::regex_match(text, plausible) ? text : std::string();
}

std::vector<std::pair<std::string, std::string>> configFilesFromQueryOutput(const std::string &text)
{
    std::vector<std::pair<std::string, std::string>> files;
    for (const std::string &line : nonEmptyLines(text)) {
        const std::vector<std::string> fields = split(line, '\t');
        if (fields.size() == 3 && contains(fields[1], "c") && !fields[2].empty()
            && fields[2].front() == '/')
            files.emplace_back(fields[0], fields[2]);
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<std::string> candidateAppNames(const std::string &packageName,
                                           const std::vector<std::string> &ownedFiles)
{
    std::set<std::string> names;
    const auto addName = [&names](const std::string &raw) {
        std::string name = trimmed(raw);
        if (name.empty())
            return;
        for (const char *suffix : {".desktop", ".appdata.xml", ".metainfo.xml"}) {
            if (endsWith(name, suffix))
                name.erase(name.size() - std::string(suffix).size());
        }
        names.insert(name);
    };
    const auto lastDotPart = [](const std::string &text) { return text.substr(text.rfind('.') + 1); };
    const auto firstDashPart = [](const std::string &text) { return text.substr(0, text.find('-')); };

    addName(packageName);
    if (contains(packageName, "-")) {
        const auto first = packageName.find_first_not_of('-');
        if (first != std::string::npos)
            addName(firstDashPart(packageName.substr(first)));
        for (const char *suffix : {"-shell", "-gui", "-app"}) {
            if (endsWith(packageName, suffix))
                addName(packageName.substr(0, packageName.size() - std::string(suffix).size()));
        }
    }

    for (const std::string &file : ownedFiles) {
        const std::string name = fileName(file);
        if (startsWith(file, "/usr/bin/") || startsWith(file, "/usr/sbin/")) {
            std::error_code error;
            if (std::filesystem::is_regular_file(file, error))
                addName(name);
        }
        if (endsWith(file, ".desktop")) {
            addName(name);
            const std::string base = name.substr(0, name.rfind('.'));
            if (contains(base, "."))
                addName(lastDotPart(base));
            if (contains(base, "-"))
                addName(firstDashPart(base));
        }
        if (endsWith(file, ".appdata.xml") || endsWith(file, ".metainfo.xml")) {
            addName(name);
            std::string base = name;
            for (const char *suffix : {".appdata.xml", ".metainfo.xml"}) {
                for (auto at = base.find(suffix); at != std::string::npos; at = base.find(suffix))
                    base.erase(at, std::string(suffix).size());
            }
            if (contains(base, "."))
                addName(lastDotPart(base));
            if (contains(base, "-"))
                addName(firstDashPart(base));
        }
    }

    std::vector<std::string> sorted(names.begin(), names.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](const std::string &a, const std::string &b) {
        return g_ascii_strcasecmp(a.c_str(), b.c_str()) < 0;
    });
    return sorted;
}

Rows userConfigRows(const std::string &packageName, const std::vector<std::string> &ownedFiles,
                    const UserFolders &folders)
{
    Rows rows;
    std::set<std::string> seenPaths;

    rows.push_back({"Note",
                    "RPM does not track files in your home folder, so these paths are guessed "
                    "from the package name, its programs, and its desktop files.",
                    "Read-only", "XDG config, data and cache folders"});

    const std::pair<const char *, std::string> roots[] = {
        {"Config", folders.config},
        {"Data", folders.data},
        {"Cache", folders.cache},
    };
    const std::vector<std::string> names = candidateAppNames(packageName, ownedFiles);
    for (const auto &[kind, root] : roots) {
        if (root.empty())
            continue;
        for (const std::string &name : names) {
            const std::string path = (std::filesystem::path(root) / name).string();
            std::error_code error;
            if (!std::filesystem::exists(path, error) || !seenPaths.insert(path).second)
                continue;
            rows.push_back({std::string("User ") + kind, path,
                            isDirectory(path) ? "Directory" : "File",
                            "Matched likely app name '" + name + "'"});
        }
    }

    if (rows.size() == 1)
        rows.push_back({"No match",
                        "No likely user config, data or cache path was found.", "None found",
                        "Checked the package name, owned programs, and desktop file IDs"});
    return rows;
}

Rows systemConfigRows(const std::string &packageName, const std::vector<std::string> &ownedFiles,
                      const std::vector<std::pair<std::string, std::string>> &dependencyConfig)
{
    Rows rows;
    std::set<std::string> seen;
    for (const std::string &path : ownedFiles) {
        if (startsWith(path, "/etc") && seen.insert(path).second)
            rows.push_back({packageName, path, isDirectory(path) ? "Directory" : "File",
                            "Owned by this package"});
    }
    for (const auto &[provider, path] : dependencyConfig) {
        if (seen.insert(path).second)
            rows.push_back({provider, path, isDirectory(path) ? "Directory" : "File",
                            "Owned by direct dependency " + provider});
    }
    if (rows.empty())
        rows.push_back({"No system config found", "—",
                        "No /etc paths were found for the package or its direct dependencies.",
                        "Checked RPM-owned /etc paths and direct dependency config"});
    return rows;
}

} // namespace dnfhelper::parsers
