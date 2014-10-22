# Created by IBM WorkFrame/2 MakeMake at 04:48:52 on 01/18/95
#
# This makefile should be run in the following directory:
#   H:\C\Edit89
#
# The actions included in this makefile are:
#   BIND::Resource Bind
#   COMPILE::CLC C++
#   COMPILE::Resource Compile
#   LINK::CLC Link

.all: \
  .\Edit89.EXE

.SUFFIXES:

.SUFFIXES: .C .rc

.rc.res:
      @echo WF::COMPILE::Resource Compile
      rc.exe -r %s %|fF.RES

.C.obj:
      @echo WF::COMPILE::CLC C++
      icc.exe /DDEBUG /Ss /Q /Wclscmpcndcnscnvcpydcleffenuextgengnrgotinilanobsordparporppcpptprorearettrdtruundusevft /Ti /W2 /C %s

.\Edit89.EXE: \
    .\Edit89.obj \
    .\Edit89IE.obj \
    .\Edit89.res \
    {$(LIB)}Edit89.DEF \
    Edit89.MAK
      @echo WF::LINK::CLC Link
      icc.exe @<<
 /B" /pmtype:pm /nologo /m"
 /FeEdit89.EXE 
 Edit89.DEF
 .\Edit89.obj
 .\Edit89IE.obj
<<
      @echo WF::BIND::Resource Bind
      rc.exe .\Edit89.res Edit89.EXE

.\Edit89.res: \
    H:\C\Edit89\Edit89.rc \
    {$(INCLUDE)}edit89.ico \
    {$(INCLUDE)}edit89.h \
    Edit89.MAK

.\Edit89.obj: \
    H:\C\Edit89\Edit89.C \
    {H:\C\Edit89;$(INCLUDE);}edit89.h \
    {H:\C\Edit89;$(INCLUDE);}rdwr89.h \
    Edit89.MAK

.\Edit89IE.obj: \
    H:\C\Edit89\Edit89IE.C \
    {H:\C\Edit89;$(INCLUDE);}edit89.h \
    {H:\C\Edit89;$(INCLUDE);}rdwr89.h \
    Edit89.MAK

