ACLOCAL_AMFLAGS = -I config

SUBDIRS = src resource qmanager doc etc t

EXTRA_DIST= \
	config/tap-driver.sh \
	NOTICE.LLNS \
	README.md \
	NEWS.md

CODE_COVERAGE_IGNORE_PATTERN = \
    "$(abs_top_builddir)/t/*" \
    "*/test/*.c" \
    "*/tests/*.c" \
    "*/common/libtap/*" \
    "*/common/libczmqcontainers/*" \
    "/usr/include/*" \
    "/usr/lib/*"
CODE_COVERAGE_LCOV_OPTIONS =
@CODE_COVERAGE_RULES@
