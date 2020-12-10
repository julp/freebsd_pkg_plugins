# Created by: Eivind Eklund <eivind@FreeBSD.org>
# $FreeBSD: head/devel/re2c/Makefile 491548 2019-01-29 09:00:37Z amdmi3 $

PORTNAME=	re2c
PORTVERSION=	2.0.3
CATEGORIES=	devel

USES=		cmake
USE_GITHUB=	yes
GH_ACCOUNT=	skvadrik
#GH_PROJECT=	re2c

MAINTAINER=	chalpin@cs.wisc.edu
COMMENT=	Compile regular expression to C (much faster final code than flex)

LICENSE=	PD

OPTIONS_DEFINE=	DOCS EXAMPLES

GNU_CONFIGURE=	yes

PORTDOCS=	*
PORTEXAMPLES=	*

post-install-DOCS-on:
	@${MKDIR} ${STAGEDIR}${DOCSDIR}/
	(cd ${WRKSRC}/doc/ && ${COPYTREE_SHARE} . ${STAGEDIR}${DOCSDIR}/)

post-install-EXAMPLES-on:
	@${MKDIR} ${STAGEDIR}${EXAMPLESDIR}/
	(cd ${WRKSRC}/examples/ && ${COPYTREE_SHARE} . ${STAGEDIR}${EXAMPLESDIR}/)

.include <bsd.port.mk>
