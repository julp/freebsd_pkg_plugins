PORTNAME=	pkg_services
PORTVERSION=	0.6.1
PORTREVISION=	1
CATEGORIES=	ports-mgmt

MAINTAINER=	not@an.address
COMMENT=	Pkg plugin to restart/stop services on update/deletion

LICENSE=	BSD2CLAUSE

USES=		cmake
CMAKE_ARGS=	-DPLUGINS:STRING=services

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_PROJECT=	freebsd_pkg_plugins
GH_TAGNAME=	1f72899

PLIST_FILES=	lib/pkg/services.so

.include <bsd.port.mk>
