# DecompMDL

DecompMDL is a tool for decompiling file formats used by GoldSrc (the Half-Life engine).

If you are new to Half-Life modding, it's recommended that you check out [TWHL's Tools & Resources](https://twhl.info/wiki/page/Tools_and_Resources) page for information on editing & compiling of its various file formats.

You can find up-to-date [Windows builds](https://github.com/Toodles2You/halflife-tools/releases) or [report issues](https://github.com/Toodles2You/halflife-tools/issues) by visiting the [GitHub repository](https://github.com/Toodles2You/halflife-tools).

## Supported file formats

### Models (.mdl)

Meshes & animations are converted to SMD files, which can be imported into various modeling suites, such as [Blender](https://www.blender.org/) via [Blender Source Tools](http://steamreview.org/BlenderSourceTools/). Textures are converted to bitmaps. The *studiomdl* script is stored in a QC file.

### Sprites (.spr)

Frames are converted to bitmaps. The *sprgen* script is stored in a QC file.

### Texture collections (.wad & .bsp)

Textures can be extracted from both WADs & BSPs containing embedded textures. They will be converted to bitmaps. Use the "-pattern" option, followed by a string, to extract only textures containing the specified substring.

## Basic usage

All that's needed is a path to the desired input file. Everything else is optional.

    Usage:
        decompmdl [options...]
        <input file> [<output directory or QC file>]
    
    Options:
        -help               Display this message & exit.
        
        -cd <path>          Sets the data path. Defaults to ".".
                            If set, data will be placed relative to root path.
        
        -cdtexture <path>   Sets the texture path, relative to data path.               
                            Defaults to "./maps_8bit" for models, & "./bmp"
                            for everything else.
        
        -cdanim <path>      Sets the animation path, relative to data path.
                            Defaults to "./anims".
        
        -pattern <string>   If set, only textures containing the matching
                            substring will be extracted from WADs & BSPs.

        -info [<string>]    File info will be printed. No decompiling will occur.

                            A comma separated list of following arguments may be
                            added to print extra info: "acts" "events" "bodygroups"

                            If the input file is a WAD or BSP, the optional string
                            will instead act identically to the "-pattern" option.
