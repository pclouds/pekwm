AUTOMAKE_OPTIONS = foreign

CXXFLAGS = @CXXFLAGS@ -DSYSCONFDIR=\"$(sysconfdir)/pekwm\" -DDATADIR=\"$(datadir)\"

bin_PROGRAMS = pekwm
pekwm_SOURCES = \
	Action.hh \
	ActionHandler.cc ActionHandler.hh \
  ActionMenu.cc ActionMenu.hh \
	Atoms.cc Atoms.hh \
  AutoProperties.cc AutoProperties.hh \
	CfgParser.cc CfgParser.hh \
	CfgParserKey.cc CfgParserKey.hh \
	CfgParserSource.cc CfgParserSource.hh \
  ColorHandler.cc ColorHandler.hh \
	Client.cc Client.hh \
	CmdDialog.cc CmdDialog.hh \
	Config.cc Config.hh \
	DecorMenu.cc DecorMenu.hh \
  DockApp.cc DockApp.hh \
	Frame.cc Frame.hh \
	FontHandler.cc FontHandler.hh \
  FrameListMenu.cc FrameListMenu.hh \
	Harbour.cc Harbour.hh \
	Handler.hh \
  HarbourMenu.cc HarbourMenu.hh \
  ImageHandler.cc ImageHandler.hh \
	KeyGrabber.cc KeyGrabber.hh \
  PDecor.cc PDecor.hh \
	PFont.cc PFont.hh \
  PMenu.cc PMenu.hh \
	PTexture.hh \
  PTexturePlain.cc PTexturePlain.hh \
	PWinObj.cc PWinObj.hh \
	PImage.hh \
	PImageNative.cc PImageNative.hh \
	PImageNativeLoader.hh \
	PImageNativeLoaderJpeg.cc PImageNativeLoaderJpeg.hh \
	PImageNativeLoaderPng.cc PImageNativeLoaderPng.hh \
	PImageNativeLoaderXpm.cc PImageNativeLoaderXpm.hh \
  PixmapHandler.cc PixmapHandler.hh \
	RegexString.cc RegexString.hh \
  ParseUtil.hh \
	PScreen.cc PScreen.hh \
	ScreenResources.cc ScreenResources.hh \
	StatusWindow.cc StatusWindow.hh \
	Theme.cc Theme.hh \
	TextureHandler.cc TextureHandler.hh \
  Types.hh \
	Util.cc Util.hh \
	Viewport.cc Viewport.hh \
  WORefMenu.cc WORefMenu.hh \
	WindowManager.cc WindowManager.hh \
  Workspaces.cc Workspaces.hh \
	main.cc \
	pekwm.hh

distclean-local:
	rm -f *\~ .\#*

