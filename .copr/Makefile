repo_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

.PHONY: srpm
srpm:
	@set -eu; \
	package_name="$$(rpmspec -q --srpm --qf '%{NAME}\n' "$(spec)" | head -n 1)"; \
	package_version="$$(rpmspec -q --srpm --qf '%{VERSION}\n' "$(spec)" | head -n 1)"; \
	source_archive="$(outdir)/v$${package_version}.tar.gz"; \
	mkdir -p "$(outdir)"; \
	tar -C "$(repo_dir)" \
		--create --gzip --file "$${source_archive}" \
		--exclude-vcs --exclude='./build' \
		--transform "s|^\./|$${package_name}-$${package_version}/|" .; \
	rpmbuild -bs "$(spec)" \
		--define "_sourcedir $(outdir)" \
		--define "_srcrpmdir $(outdir)"
