












@echo OFF

cd ..\gtksourceview

if exist gtksourceview-enumtypes.h del gtksourceview-enumtypes.h
if exist gtksourceview-enumtypes.c del gtksourceview-enumtypes.c

set PYTHON=%2

if "%PYTHON%" == "" goto use_perl

call %PYTHON% %1\bin\glib-mkenums ^
--fhead "#ifdef HAVE_CONFIG_H\n" ^
--fhead "#include \"config.h\"\n" ^
--fhead "#endif\n\n" ^
--fhead "#include <glib-object.h>\n" ^
--fhead "#include \"gtksourceview-enumtypes.h\"\n\n" ^
--fprod "\n/* enumerations from \"@filename@\" */" ^
--vhead "static const G@Type@Value _@enum_name@_values[] = {" ^
--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," ^
--vtail "  { 0, NULL, NULL }\n};\n\n" ^
--vtail "GType\n@enum_name@_get_type (void)\n{\n" ^
--vtail "  static GType type = 0;\n\n" ^
--vtail "  if (!type)\n" ^
--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" ^
--vtail "  return type;\n}\n\n" ^
gtksource.h ^
gtksourceautocleanups.h ^
gtksourcebuffer.h ^
gtksourcecompletion.h ^
gtksourcecompletioncontext.h ^
gtksourcecompletioninfo.h ^
gtksourcecompletionitem.h ^
gtksourcecompletionproposal.h ^
gtksourcecompletionprovider.h ^
gtksourceencoding.h ^
gtksourcefile.h ^
gtksourcefileloader.h ^
gtksourcefilesaver.h ^
gtksourcegutter.h ^
gtksourcegutterrenderer.h ^
gtksourcegutterrendererpixbuf.h ^
gtksourcegutterrenderertext.h ^
gtksourcelanguage.h ^
gtksourcelanguagemanager.h ^
gtksourcemap.h ^
gtksourcemark.h ^
gtksourcemarkattributes.h ^
gtksourceprintcompositor.h ^
gtksourceregion.h ^
gtksourcesearchcontext.h ^
gtksourcesearchsettings.h ^
gtksourcespacedrawer.h ^
gtksourcestyle.h ^
gtksourcestylescheme.h ^
gtksourcestyleschemechooser.h ^
gtksourcestyleschemechooserbutton.h ^
gtksourcestyleschemechooserwidget.h ^
gtksourcestyleschememanager.h ^
gtksourcetag.h ^
gtksourcetypes.h ^
gtksourceundomanager.h ^
gtksourceutils.h ^
gtksourceview.h ^
gtksourceview-typebuiltins.h ^
&1> gtksourceview-enumtypes.c

call %PYTHON% %1\bin\glib-mkenums ^
--fhead "#ifndef GTKSOURCEVIEW_ENUMTYPES_H\n" ^
--fhead "#define GTKSOURCEVIEW_ENUMTYPES_H\n\n" ^
--ftail "#endif /* GTKSOURCEVIEW_ENUMTYPES_H */\n" ^
--fprod "#include <gtksourceview/@filename@>\n" ^
--eprod "G_BEGIN_DECLS\n" ^
--eprod "#define GTK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" ^
--eprod "GTK_SOURCE_ENUM_EXTERN\nGType @enum_name@_get_type (void);\n" ^
--eprod "G_END_DECLS\n\n" ^
gtksource.h ^
gtksourceautocleanups.h ^
gtksourcebuffer.h ^
gtksourcecompletion.h ^
gtksourcecompletioncontext.h ^
gtksourcecompletioninfo.h ^
gtksourcecompletionitem.h ^
gtksourcecompletionproposal.h ^
gtksourcecompletionprovider.h ^
gtksourceencoding.h ^
gtksourcefile.h ^
gtksourcefileloader.h ^
gtksourcefilesaver.h ^
gtksourcegutter.h ^
gtksourcegutterrenderer.h ^
gtksourcegutterrendererpixbuf.h ^
gtksourcegutterrenderertext.h ^
gtksourcelanguage.h ^
gtksourcelanguagemanager.h ^
gtksourcemap.h ^
gtksourcemark.h ^
gtksourcemarkattributes.h ^
gtksourceprintcompositor.h ^
gtksourceregion.h ^
gtksourcesearchcontext.h ^
gtksourcesearchsettings.h ^
gtksourcespacedrawer.h ^
gtksourcestyle.h ^
gtksourcestylescheme.h ^
gtksourcestyleschemechooser.h ^
gtksourcestyleschemechooserbutton.h ^
gtksourcestyleschemechooserwidget.h ^
gtksourcestyleschememanager.h ^
gtksourcetag.h ^
gtksourcetypes.h ^
gtksourceundomanager.h ^
gtksourceutils.h ^
gtksourceview.h ^
gtksourceview-typebuiltins.h ^
&1> gtksourceview-enumtypes.h.tmp

:use_perl
set f=gtksourceview-enumtypes.c
if not exist %f% goto do_enum_c
for %%x in (%f%) do if %%~zx gtr 0 goto done_enum_c

:do_enum_c

call perl %1\bin\glib-mkenums ^
--fhead "#ifdef HAVE_CONFIG_H\n" ^
--fhead "#include \"config.h\"\n" ^
--fhead "#endif\n\n" ^
--fhead "#include <glib-object.h>\n" ^
--fhead "#include \"gtksourceview-enumtypes.h\"\n\n" ^
--fprod "\n/* enumerations from \"@filename@\" */" ^
--vhead "static const G@Type@Value _@enum_name@_values[] = {" ^
--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," ^
--vtail "  { 0, NULL, NULL }\n};\n\n" ^
--vtail "GType\n@enum_name@_get_type (void)\n{\n" ^
--vtail "  static GType type = 0;\n\n" ^
--vtail "  if (!type)\n" ^
--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" ^
--vtail "  return type;\n}\n\n" ^
gtksource.h ^
gtksourceautocleanups.h ^
gtksourcebuffer.h ^
gtksourcecompletion.h ^
gtksourcecompletioncontext.h ^
gtksourcecompletioninfo.h ^
gtksourcecompletionitem.h ^
gtksourcecompletionproposal.h ^
gtksourcecompletionprovider.h ^
gtksourceencoding.h ^
gtksourcefile.h ^
gtksourcefileloader.h ^
gtksourcefilesaver.h ^
gtksourcegutter.h ^
gtksourcegutterrenderer.h ^
gtksourcegutterrendererpixbuf.h ^
gtksourcegutterrenderertext.h ^
gtksourcelanguage.h ^
gtksourcelanguagemanager.h ^
gtksourcemap.h ^
gtksourcemark.h ^
gtksourcemarkattributes.h ^
gtksourceprintcompositor.h ^
gtksourceregion.h ^
gtksourcesearchcontext.h ^
gtksourcesearchsettings.h ^
gtksourcespacedrawer.h ^
gtksourcestyle.h ^
gtksourcestylescheme.h ^
gtksourcestyleschemechooser.h ^
gtksourcestyleschemechooserbutton.h ^
gtksourcestyleschemechooserwidget.h ^
gtksourcestyleschememanager.h ^
gtksourcetag.h ^
gtksourcetypes.h ^
gtksourceundomanager.h ^
gtksourceutils.h ^
gtksourceview.h ^
gtksourceview-typebuiltins.h ^
&1> gtksourceview-enumtypes.c

:done_enum_c
set f=gtksourceview-enumtypes.h.tmp

if not exist %f% goto do_enum_h
for %%x in (%f%) do if %%~zx gtr 0 goto done_enum_h

:do_enum_h
call perl %1\bin\glib-mkenums ^
--fhead "#ifndef GTKSOURCEVIEW_ENUMTYPES_H\n" ^
--fhead "#define GTKSOURCEVIEW_ENUMTYPES_H\n\n" ^
--ftail "#endif /* GTKSOURCEVIEW_ENUMTYPES_H */\n" ^
--fprod "#include <gtksourceview/@filename@>\n" ^
--eprod "G_BEGIN_DECLS\n" ^
--eprod "#define GTK_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" ^
--eprod "GTK_SOURCE_ENUM_EXTERN\nGType @enum_name@_get_type (void);\n" ^
--eprod "G_END_DECLS\n\n" ^
gtksource.h ^
gtksourceautocleanups.h ^
gtksourcebuffer.h ^
gtksourcecompletion.h ^
gtksourcecompletioncontext.h ^
gtksourcecompletioninfo.h ^
gtksourcecompletionitem.h ^
gtksourcecompletionproposal.h ^
gtksourcecompletionprovider.h ^
gtksourceencoding.h ^
gtksourcefile.h ^
gtksourcefileloader.h ^
gtksourcefilesaver.h ^
gtksourcegutter.h ^
gtksourcegutterrenderer.h ^
gtksourcegutterrendererpixbuf.h ^
gtksourcegutterrenderertext.h ^
gtksourcelanguage.h ^
gtksourcelanguagemanager.h ^
gtksourcemap.h ^
gtksourcemark.h ^
gtksourcemarkattributes.h ^
gtksourceprintcompositor.h ^
gtksourceregion.h ^
gtksourcesearchcontext.h ^
gtksourcesearchsettings.h ^
gtksourcespacedrawer.h ^
gtksourcestyle.h ^
gtksourcestylescheme.h ^
gtksourcestyleschemechooser.h ^
gtksourcestyleschemechooserbutton.h ^
gtksourcestyleschemechooserwidget.h ^
gtksourcestyleschememanager.h ^
gtksourcetag.h ^
gtksourcetypes.h ^
gtksourceundomanager.h ^
gtksourceutils.h ^
gtksourceview.h ^
gtksourceview-typebuiltins.h ^
&1> gtksourceview-enumtypes.h.tmp

:done_enum_h

if "%PYTHON%" == "" goto replace_perl
call %PYTHON% ..\win32\replace.py --action=replace-str ^
-i=gtksourceview-enumtypes.h.tmp ^
-o=gtksourceview-enumtypes.h ^
--instring=GTK_TYPE_SOURCE_ ^
--outstring=GTK_SOURCE_TYPE_

goto cleanup
:replace_perl
call perl -p -e "s/GTK_TYPE_SOURCE_/GTK_SOURCE_TYPE_/g" < gtksourceview-enumtypes.h.tmp > gtksourceview-enumtypes.h

:cleanup
del gtksourceview-enumtypes.h.tmp
