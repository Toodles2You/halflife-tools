# DecompMDL

*DecompMDL* is a tool for decompiling various file formats used by *GoldSrc* (the *Half-Life* engine). It began as just a model decompiler (hence the name) but, ended up being expanded to support more.

## Supported file formats

Information that can be recovered from the compressed binary files will be extracted & stored in such a way that allows for immediate recompiling using the tools found in the *Half-Life SDK* (or adjacent community-made tools).

If you are new to *Half-Life* modding, it is recommended you check out *TWHL*'s [Tools & Resources](https://twhl.info/wiki/page/Tools_and_Resources) page for information on editing and compiling of the various file formats.

### Models (.mdl)

Meshes & animations are converted to StudoModel (.smd) files, which can be imported into various modeling suites, such as [*Blender*](https://www.blender.org/) via [*Blender Source Tools*](http://steamreview.org/BlenderSourceTools/). Textures are converted to 8-bit bitmap (.bmp) files. *studiomdl* script is stored in a QC (.qc) file.

### Sprites (.spr)

Frames are converted to 8-bit bitmap (.bmp) files. *sprgen* script is stored in a QC (.qc) file.

### Texture collections (.wad & .bsp)

Textures can be extracted from both texture WADs (.wad) & game levels (.bsp) that have textures embedded in them. They will be converted to 8-bit bitmap (.bmp) files. Use the "-pattern" option, followed by a string, to extract only textures containing the specified substring.

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
