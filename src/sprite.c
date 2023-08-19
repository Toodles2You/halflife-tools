/*
===========================================================================
Copyright (C) 1996-2002, Valve LLC. All rights reserved.
Copyright (C) 2023 Toodles

This product contains software technology licensed from Id 
Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
All Rights Reserved.

Use, distribution, and modification of this source code and/or resulting
object code is restricted to non-commercial enhancements to products from
Valve LLC.  All other use, distribution, or modification is prohibited
without written permission from Valve LLC.
===========================================================================
*/

#include "sprite.h"

void decomp_writebmp (FILE *bmp, byte *data, int width, int height, byte *palette);

static char *spr_gettype (int type)
{
    switch (type)
    {
    case SPR_VP_PARALLEL_UPRIGHT: return "vp_parallel_upright";
    case SPR_FACING_UPRIGHT: return "facing_upright";
    case SPR_VP_PARALLEL: return "vp_parallel";
    case SPR_ORIENTED: return "oriented";
    case SPR_VP_PARALLEL_ORIENTED: return "vp_parallel_oriented";
    }
    return "";
}

static char *spr_gettextureformat (int type)
{
    switch (type)
    {
    case SPR_NORMAL: return "normal";
    case SPR_ADDITIVE: return "additive";
    case SPR_INDEXALPHA: return "indexalpha";
    case SPR_ALPHTEST: return "alphatest";
    }
    return "";
}

void decomp_writesprframe (
    FILE *spr,
    const char *bmpdir,
    const char *frame_name,
    dspriteframe_t *frame,
    byte *palette)
{   
    byte *data = (byte *)memalloc (frame->width * frame->height, 1);
    
    mdl_read (spr, data, frame->width * frame->height);

    FILE *bmp = qc_open (bmpdir, frame_name, "bmp");

    decomp_writebmp (bmp, data, frame->width, frame->height, palette);

    fprintf (stdout, "Done!\n");

    free (data);
    fclose (bmp);
}

void decomp_sprframe (
    FILE *spr,
    FILE *qc,
    const char *cdtexture,
    const char *bmpdir,
    byte *palette,
    dspriteframe_t *frame,
    float interval,
    const char *frame_name)
{
    qc_writef (qc, "$load  %s/%s.bmp", cdtexture, frame_name);
    qc_write2f (qc, "$frame   0   0 %3i %3i", frame->width, frame->height);

    if (interval != 0.1F)
    {
        qc_write2f (qc, " %g", interval);
    }

    if (frame->origin[0] != -(frame->width >> 1) || frame->origin[1] != (frame->height >> 1))
    {
        if (interval == 0.1F)
        {
            qc_write2f (qc, " %g", interval);
        }
        
        qc_write2f (qc, " %3i %3i", -frame->origin[0], frame->origin[1]);
    }

    qc_putc (qc, '\n');

    decomp_writesprframe (spr, bmpdir, frame_name, frame, palette);
}

void decomp_spr (
    const char *sprname,
    const char *qcname,
    const char *cdtexture,
    const char *qcdir,
    const char *bmpdir)
{
    int id;
    int version;
    FILE *spr = mdl_open (sprname, &id, &version, false);

    if (id != IDSPRITEHEADER)
        error (1, "Not a Valve SPR\n");
    
    if (version != SPRITE_VERSION)
        error (1, "Wrong SPR version: %i\n", version);
    
    FILE *qc = qc_open (qcdir, qcname, "qc");

    dsprite_t header;
    mdl_read (spr, &header, sizeof (header));

    qc_write (qc, "/*");
    qc_write (qc, "==============================================================================");
    qc_writef (qc, "%s", skippath (sprname));
    qc_write (qc, "==============================================================================");
    qc_write (qc, "*/");
    qc_putc (qc, '\n');

    fixpath (sprname, true);
    stripext (sprname);
    sprname = skippath (sprname);
    qc_writef (qc, "$spritename %s", sprname);
    qc_writef (qc, "$type %s", spr_gettype (header.type));
    qc_writef (qc, "$texture %s", spr_gettextureformat (header.texFormat));

    if (header.beamlength != 0.0F)
    {
        qc_writef (qc, "$beamlength %g", header.beamlength);
    }

    if (header.synctype == ST_SYNC)
    {
        qc_write (qc, "$sync");
    }
    
    qc_putc (qc, '\n');

    short colors;
    mdl_read (spr, &colors, sizeof (colors));

    byte *palette = (byte *)memalloc (colors * 3, 1);

    mdl_read (spr, palette, colors * 3);

    int i, j;
    dspriteframetype_t frametype;
    dspriteframe_t frame;

    if (header.numframes == 1)
    {
        mdl_read (spr, &frametype, sizeof (frametype));
        mdl_read (spr, &frame, sizeof (frame));
        decomp_sprframe (spr, qc, cdtexture, bmpdir, palette, &frame, 0.1F, sprname);
        goto sprite_done;
    }

    dspritegroup_t group;
    float interval_total;
    dspriteinterval_t* intervals = memalloc (header.numframes, sizeof (dspriteinterval_t));
    char *frame_name = (char *)memalloc (strlen (sprname) + 8, 1);
    int framenum = 0;

    /*
        Toodles TODO: Sprites can have up to 1000 frames, so maybe
        more leading zeros should optionally be added to the BMP names.
    */
    char c = sprname[strlen (sprname) - 1];
    char frame_format[16] = "%s";

    /* If the sprite name ends with a number, add an underscore for clarity. */
    if (c >= '0' && c <= '9')
        strcat (frame_format, "_");
    
    strcat (frame_format, "%.02i");
    
    for (i = 0; i < header.numframes; ++i)
    {
        mdl_read (spr, &frametype, sizeof (frametype));

        if (frametype.type == SPR_SINGLE)
        {
            ++framenum;
            sprintf (frame_name, frame_format, sprname, framenum);
            mdl_read (spr, &frame, sizeof (frame));
            decomp_sprframe (spr, qc, cdtexture, bmpdir, palette, &frame, 0.1F, frame_name);
            continue;
        }

        qc_putc (qc, '\n');
        qc_write (qc, "$groupstart");
        
        mdl_read (spr, &group, sizeof (group));

        interval_total = 0.0F;
        for (j = 0; j < group.numframes; ++j)
        {
            mdl_read (spr, intervals + j, sizeof (*intervals));
            intervals[j].interval -= interval_total;
            interval_total += intervals[j].interval;
        }

        for (j = 0; j < group.numframes; ++j)
        {
            ++framenum;
            sprintf (frame_name, frame_format, sprname, framenum);
            mdl_read (spr, &frame, sizeof (frame));
            decomp_sprframe (spr, qc, cdtexture, bmpdir, palette, &frame, intervals[j].interval, frame_name);
        }

        qc_write (qc, "$groupend");
        qc_putc (qc, '\n');
    }

    free (frame_name);
    free (intervals);

sprite_done:

    qc_putc (qc, '\n');

    free (palette);
    
    fclose (qc);
    fclose (spr);

    fprintf (stdout, "Done!\n");
}
