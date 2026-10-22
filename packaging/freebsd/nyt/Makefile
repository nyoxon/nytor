PORTNAME=	nyt
DISTVERSION=	1.2.1
CATEGORIES=	editors

MAINTAINER=	nyoxon@duck.com
COMMENT=	Terminal-based text editor designed for programming
WWW=		https://github.com/nyoxon/nytor

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/LICENSE

USES=		gmake
USE_GITHUB=	yes
GH_ACCOUNT=	nyoxon
GH_PROJECT=	nytor
# USE_GITHUB defaults GH_TAGNAME to DISTVERSION; publish that Git tag first.

ALL_TARGET=	all plugins
INSTALL_TARGET=	install
MAKE_ARGS=	CC="${CC}" PREFIX="${PREFIX}"

pre-build:
	${SH} ${WRKSRC}/scripts/sync_version.sh --check

PLIST_FILES=	bin/nyt \
		share/man/man1/nyt.1 \
		share/nytor/config.ny \
		share/nytor/themes/TempleOs \
		share/nytor/themes/default \
		share/nytor/themes/default_dark \
		share/nytor/themes/default_light \
		share/nytor/plugins/file_stats.lua \
		share/nytor/plugins/longest_line.lua \
		share/nytor/plugins/trim_trailing_whitespace.lua \
		share/nytor/plugins/languages/asm_x86.so \
		share/nytor/plugins/languages/bash.so \
		share/nytor/plugins/languages/c.so \
		share/nytor/plugins/languages/gdscript.so \
		share/nytor/plugins/languages/lua.so \
		share/nytor/plugins/languages/make.so \
		share/nytor/plugins/languages/rust.so

.include <bsd.port.mk>
