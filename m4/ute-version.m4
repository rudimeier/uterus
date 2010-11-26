dnl ute-version.m4 -- estimate version from git infos
dnl
dnl Copyright (C) 2010  Sebastian Freundt.
dnl
dnl This file is part of uterus

dnl UTE_CHECK_VERSION( SCRIPTNAME, VERSIONFILE )
AC_DEFUN([UTE_CHECK_VERSION], [
	pushdef([SCRNAME], [$1])

	AC_MSG_CHECKING([for ]${PACKAGE_NAME}[ version])
	if test -f "[]SCRNAME[]"; then
		VERSION=$(SCRNAME)
	elif test -f "${ac_top_srcdir}[]SCRNAME[]"; then
		VERSION=$("${ac_top_srcdir}[]SCRNAME[]")
	else
		VERSION="unknown"
	fi
	AC_MSG_RESULT([${]VERSION[}])

	PACKAGE_VERSION="${VERSION}"
	popdef([SCRNAME])
])dnl UTE_CHECK_VERSION

dnl ute-version.m4 ends here
