PORTNAME=	pkg_zint
PORTVERSION=	0.4.0
PORTREVISION=	2
CATEGORIES=	ports-mgmt

MAINTAINER=	not@an.address
COMMENT=	Pkg plugin to automatically "snapshot" (BE or raw ZFS) before upgrade

LICENSE=	BSD2CLAUSE

USES=		cmake
CMAKE_ARGS=	-DPLUGINS:STRING=zint

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_PROJECT=	freebsd_pkg_plugins
GH_TAGNAME=	1f72899

PLIST_FILES=	lib/pkg/zint.so

.include <bsd.port.mk>
