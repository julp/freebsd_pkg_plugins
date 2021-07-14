PORTNAME=	pkg_history
PORTVERSION=	0.8.0
#PORTREVISION=	1
CATEGORIES=	ports-mgmt

MAINTAINER=	not@an.address
COMMENT=	Pkg plugin to historize pkg commands and their effects

LICENSE=	BSD2CLAUSE

BUILD_DEPENDS=	re2c:devel/re2c

USES=		cmake sqlite
#CMAKE_ON=
#CMAKE_OFF=
#CMAKE_ARGS=

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_PROJECT=	freebsd_pkg_plugins
GH_TAGNAME=	e320b5b

PLIST_FILES=	lib/pkg/history.so

.include <bsd.port.mk>
