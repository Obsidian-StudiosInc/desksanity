AC_INIT(desksanity, 1.1.0, zmike@enlightenment.org)
AC_CONFIG_MACRO_DIR([m4])
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE([1.8 dist-xz])
AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_STDC

define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL

#m4_ifdef([AM_GNU_GETTEXT_VERSION], [
#AM_GNU_GETTEXT_VERSION([0.14])
#])

#m4_ifdef([AM_GNU_GETTEXT], [
#AM_GNU_GETTEXT([external])
#po_makefile_in=po/Makefile.in
#AM_CONDITIONAL([HAVE_PO], [true])
#],[
#AM_CONDITIONAL([HAVE_PO], [false])
#])
#AC_SUBST(LTLIBINTL)

PKG_CHECK_EXISTS([efl-wl], [build_runner="yes"], [build_runner="no"])

if test "x$build_runner" = "xyes" ; then
  runner="efl-wl"
  AC_DEFINE_UNQUOTED(HAVE_RUNNER, 1, "have runner deps")
fi
AM_CONDITIONAL([BUILD_RUNNER], [test "x${build_runner}" = "xyes"])

PKG_CHECK_MODULES([E], [enlightenment >= 0.21.0 $runner])
release=$(pkg-config --variable=release enlightenment)
MODULE_ARCH="$host_os-$host_cpu-$release"
AC_SUBST(MODULE_ARCH)
AC_DEFINE_UNQUOTED(MODULE_ARCH, "$MODULE_ARCH", "Module architecture")

module_dir="$(pkg-config --variable=modules enlightenment)"
AC_SUBST(module_dir)
datadir="$module_dir/desksanity"

AC_OUTPUT([
Makefile
e_modules-desksanity.spec
], [
])

#$po_makefile_in

