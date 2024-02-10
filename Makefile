all:
	cd mylibffi && cargo build
	cd cmd/rsclient && cargo build
	test -f cmd/cppclient/build/build.ninja || (cd cmd/cppclient && meson setup build)
	cd cmd/cppclient && meson compile -C build
.PHONY:	all

test:
	cd mylib && cargo test
	cd cmd/rsclient && cargo run
	cd cmd/cppclient && meson compile -C build && ./build/ffitest
.PHONY:	test

lint:
	cd mylib; cargo fmt --all --check; cargo clippy --no-deps --tests --examples
	cd mylibffi; cargo fmt --all --check; cargo clippy --no-deps --tests --examples
	cd cmd/rsclient; cargo fmt --all --check; cargo clippy --no-deps --tests --examples
	cd cmd/cppclient; clang-format --dry-run --Werror $$(git ls-files | grep '\.[ch]pp$$'); \
		clang-tidy -p build $$(git ls-files | grep '\.[ch]pp$$')
.PHONY:	lint

fmt:
	cd mylib; cargo fmt --all
	cd mylibffi; cargo fmt --all
	cd cmd/rsclient; cargo fmt --all
	cd cmd/cppclient; clang-format -i $$(git ls-files | grep '\.[ch]pp$$')
.PHONY:	fmt

clean:
	cd mylib && cargo clean
	cd mylibffi && cargo clean
	cd cmd/rsclient && cargo clean
	rm -rf cmd/cppclient/build
.PHONY:	clean
