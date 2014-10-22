# Created by IBM WorkFrame/2 MakeMake at 04:51:25 on 01/18/95
#
# This makefile should be run in the following directory:
#   H:\C\Edit89
#
# The actions included in this makefile are:
#   COMPILE::CLC C++
#   LINK::CLC Link

.all: \
  .\RdWr89.DLL

.SUFFIXES:

.SUFFIXES: .C

.C.obj:
      @echo WF::COMPILE::CLC C++
      icc.exe /DDEBUG /Ss /Q /Wclscmpcndcnscnvcpydcleffenuextgengnrgotinilanobsordparporppcpptprorearettrdtruundusevft /Ti /W2 /Gm /Ge- /C %s

.\RdWr89.DLL: \
    .\RdWr8.obj \
    .\RdWr9.obj \
    {$(LIB)}RdWr89.DEF \
    RdWr89.MAK
      @echo WF::LINK::CLC Link
      icc.exe @<<
 /B" /nologo /m"
 /FeRdWr89.DLL 
 RdWr89.DEF
 .\RdWr8.obj
 .\RdWr9.obj
<<

.\RdWr8.obj: \
    H:\C\Edit89\RdWr8.C \
    {H:\C\Edit89;$(INCLUDE);}rdwr89.h \
    RdWr89.MAK

.\RdWr9.obj: \
    H:\C\Edit89\RdWr9.C \
    {H:\C\Edit89;$(INCLUDE);}rdwr89.h \
    RdWr89.MAK

