/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef PROTO_OPENPCI_H
#define PROTO_OPENPCI_H

#include <clib/openpci_protos.h>

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/openpci.h>
#  else
#   include <inline/openpci.h>
#  endif
# else
#  include <pragmas/openpci_pragmas.h>
# endif
#endif /* _NO_INLINE */

#ifdef __amigaos4__
# include <interfaces/openpci.h>
# ifndef __NOGLOBALIFACE__
   extern struct OpenPciIFace *IOpenPci;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern struct Library *
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  OpenPciBase;
#endif /* !__NOLIBBASE__ */

#endif /* !PROTO_OPENPCI_H */
