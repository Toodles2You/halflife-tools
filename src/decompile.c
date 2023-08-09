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

#ifdef __GNUC__
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <memory.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "decompile.h"
#include "studio.h"

void decomp_studiomodel (
    FILE *mdl,
    const char *smddir,
    studiohdr_t *header,
    mstudiomodel_t *model);

void decomp_studiotexture (
    FILE *mdl,
    const char *bmpdir,
    studiohdr_t *header,
    mstudiotexture_t *texture);

static void decomp_writeinfo (
    FILE *mdl,
    FILE *qc,
    const char *cd,
    const char *cdtexture,
    studiohdr_t *header)
{
    qc_write (qc, "/*");
    qc_write (qc, "==============================================================================");
    qc_writef (qc, "%s", skippath (header->name));
    qc_write (qc, "==============================================================================");
    qc_write (qc, "*/");
    qc_putc (qc, '\n');

    /* Toodles: Keep the original output name because GoldSrc's filesystem is case-sensitive. */

    qc_writef (qc, "$modelname %s", header->name);
    qc_writef (qc, "$cd %s", cd);
    qc_writef (qc, "$cdtexture %s", cdtexture);
    qc_putc (qc, '\n');

    if (vec3_nonzero (header->eyeposition))
    {
        qc_writef (qc, "$eyeposition %g %g %g", vec3_print (header->eyeposition));
        qc_putc (qc, '\n');
    }
}

static void decomp_writebodygroups (FILE *mdl, FILE *qc, const char *smddir, studiohdr_t *header)
{
    int i, j;
    mstudiobodyparts_t bodypart;
    mstudiomodel_t model;
    bool group;

    for (i = 0; i < header->numbodyparts; ++i)
    {
        mdl_seek (mdl, header->bodypartindex + sizeof (bodypart) * i, SEEK_SET);
        mdl_read (mdl, &bodypart, sizeof (bodypart));

        group = bodypart.nummodels > 1;

        if (group)
        {
            qc_writef (qc, "$bodygroup %s", bodypart.name);
            qc_putc (qc, '{');
            qc_putc (qc, '\n');
        }

        mdl_seek (mdl, bodypart.modelindex, SEEK_SET);

        for (j = 0; j < bodypart.nummodels; ++j)
        {
            mdl_read (mdl, &model, sizeof(model));

            fixpath (model.name, true);

            if (!strcasecmp (model.name, "blank"))
            {
                qc_write (qc, group ? "    blank" : "blank");
                continue;
            }
            
            qc_writef (
                qc,
                group ? "    studio \"%s\"" : "$body studio \"%s\"",
                model.name);

            decomp_studiomodel (mdl, smddir, header, &model);
        }

        if (group)
        {
            qc_putc (qc, '}');
            qc_putc (qc, '\n');
        }
    }

    qc_putc (qc, '\n');
}

static bool decomp_buildskingroups (
    FILE *mdl,
    FILE *qc,
    studiohdr_t *header,
    short** out_skin_index,
    bool** out_skin)
{
    int i, j;

    int families = header->numskinfamilies;
    int refs = header->numskinref;
    bool any_skins = false;

    bool *skin = (bool *)calloc (refs, sizeof (*skin));

    short *skin_index = (short *)calloc (families * refs, sizeof (*skin_index));

    mdl_seek (mdl, header->skinindex, SEEK_SET);
    mdl_read (mdl, skin_index, families * refs * sizeof (*skin_index));

    *out_skin_index = skin_index;
    *out_skin = skin;

    /*
    Toodles TODO: This should recreate all the original skin groups.
    They aren't actually needed; It's just for accuracy and clarity.
    */

    for (i = 0; i < families; ++i)
    {
        for (j = 0; j < refs; ++j)
        {
            if (!skin[j] && i > 0
                && skin_index[i * refs + j] != skin_index[(i - 1) * refs + j])
            {
                skin[j] = true;
                any_skins = true;
            }
        }
    }

    return any_skins;
}

static void decomp_writeskingroups (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    short* skin_index;
    bool* skin;

    if (!decomp_buildskingroups (mdl, qc, header, &skin_index, &skin))
        goto skins_done;

    qc_write (qc, "$texturegroup group");
    qc_putc (qc, '{');
    qc_putc (qc, '\n');

    int i, j;

    char texture_name[64];

    for (i = 0; i < header->numskinref; ++i)
    {
        qc_write2f (qc, "    { ");

        for (j = 0; j < header->numskinfamilies; ++j)
        {
            if (!skin[j])
                continue;
            
            mdl_seek (
                mdl,
                header->textureindex + sizeof (mstudiotexture_t) * skin_index[i * header->numskinref + j],
                SEEK_SET);
            
            mdl_read (mdl, &texture_name, sizeof (texture_name));
            
            fixpath (texture_name, true);
            stripext (texture_name);
            
            qc_write2f (qc, "\"%s.bmp\" ", skippath (texture_name));
        }

        qc_putc (qc, '}');
        qc_putc (qc, '\n');
    }

    qc_putc (qc, '}');
    qc_putc (qc, '\n');

skins_done:
    free (skin);
    free (skin_index);
}

static void decomp_writeattachments (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numattachments <= 0)
        return;
    
    int i;

    mstudioattachment_t attachment;
    char bone_name[32];

    for (i = 0; i < header->numattachments; ++i)
    {
        mdl_seek (mdl, header->attachmentindex + sizeof (attachment) * i, SEEK_SET);
        mdl_read (mdl, &attachment, sizeof (attachment));

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * attachment.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));
        
        qc_writef (qc, "$attachment %i \"%s\" %g %g %g", i, bone_name, vec3_print(attachment.org));
    }
    
    qc_putc (qc, '\n');
}

static void decomp_writecontrollers (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numbonecontrollers <= 0)
        return;
    
    int i;

    mstudiobonecontroller_t ctrl;
    char bone_name[32];

    for (i = 0; i < header->numbonecontrollers; ++i)
    {
        mdl_seek (mdl, header->bonecontrollerindex + sizeof (ctrl) * i, SEEK_SET);
        mdl_read (mdl, &ctrl, sizeof (ctrl));

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * ctrl.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));

        qc_write2f (qc, "$controller ");

        if (ctrl.index == 4)
        {
            qc_write2f (qc, "mouth ");
        }
        else
        {
            qc_write2f (qc, "%i ", ctrl.index);
        }

        qc_writef (
            qc,
            "\"%s\" %s %g %g",
            bone_name,
            mdl_getmotionflag (ctrl.type),
            ctrl.start,
            ctrl.end);
    }
    
    qc_putc (qc, '\n');
}

static void decomp_writehitboxes (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numhitboxes <= 0)
        return;
    
    int i;

    mstudiobbox_t hbox;
    char bone_name[32];
    bool has_hbox = false;

    for (i = 0; i < header->numhitboxes; ++i)
    {
        mdl_seek (mdl, header->hitboxindex + sizeof (hbox) * i, SEEK_SET);
        mdl_read (mdl, &hbox, sizeof (hbox));

        /* Toodles TODO: This skips auto generated boxes. It might not be a good idea. */
        if (hbox.group == 0)
            continue;
        
        has_hbox = true;

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * hbox.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));

        qc_writef (
            qc,
            "$hbox %i \"%s\" %g %g %g  %g %g %g",
            hbox.group, bone_name,
            vec3_print(hbox.bbmin),
            vec3_print(hbox.bbmax));
    }

    if (has_hbox)
        qc_putc (qc, '\n');
}

static bool decomp_simplesequence (mstudioseqdesc_t *seq)
{
    return seq->numevents <= 0
        && seq->numblends <= 1
        && seq->activity <= 0
        && seq->numpivots <= 0
        && seq->blendtype[0] == 0
        && seq->blendtype[1] == 0
        && seq->entrynode == 0
        && seq->exitnode == 0
        && seq->nodeflags == 0
        && (seq->motiontype & STUDIO_TYPES) == 0;
}

static void decomp_writeseqblends (FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->numblends <= 1)
    {
        qc_writef (qc, "    \"anims/%s\"", seq->label);
    }
    else if (seq->numblends == 2)
    {
        /* I'm certain this is always the case with official models. */
        qc_writef (qc, "    \"anims/%s_down\"", seq->label);
        qc_writef (qc, "    \"anims/%s_up\"", seq->label);
    }
    else
    {
        int i;
        for (i = 0; i < seq->numblends; ++i)
        {
            qc_writef (qc, "    \"anims/%s_blend_%.02i\"", seq->label, i);
        }
    }

    qc_writef (qc, "    fps %g", seq->fps);
    
    if (seq->flags & STUDIO_LOOPING)
    {
        qc_write (qc, "    loop");
    }
}

static void decomp_writeseqact (FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->activity == 0)
        return;

    qc_write2f (qc, "    %s", mdl_getactname (seq->activity));

    if (seq->actweight != 0)
    {
        qc_write2f (qc, " %i", seq->actweight);
    }

    qc_putc (qc, '\n');
}

static void decomp_writeseqpivots (FILE *mdl, FILE *qc, mstudioseqdesc_t *seq)
{
    /* Toodles: This seems to be an unfinished feature. Keeping it because StudioMDL does write it. */
    if (seq->numpivots <= 0)
        return;

    int i;
    mstudiopivot_t pivot;
    
    mdl_seek (mdl, seq->pivotindex, SEEK_SET);
    
    for (i = 0; i < seq->numpivots; ++i)
    {
        mdl_read (mdl, &pivot, sizeof (pivot));
        qc_writef (qc, "    pivot %i %i %i", i, pivot.start, pivot.end);
    }
}

static void decomp_writeseqblendtype (FILE *qc, mstudioseqdesc_t *seq)
{
    int i;
    
    for (i = 0; i < 2; ++i)
    {
        if (seq->blendtype[i] == 0)
            continue;
        
        qc_writef (
            qc,
            "    blend %s %g %g",
            mdl_getmotionflag (seq->blendtype[i]),
            seq->blendstart[i],
            seq->blendend[i]);
    }
}

static void decomp_writeseqnodes (FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->entrynode == 0
            && seq->exitnode == 0
            && seq->nodeflags == 0)
        return;
    
    /* Toodles TODO: I haven't tested this. */
    if (seq->nodeflags != 0)
    {
        qc_writef (qc, "    rtransition %i %i", seq->entrynode, seq->exitnode);
    }
    else if (seq->entrynode == seq->exitnode)
    {
        qc_writef (qc, "    node %i", seq->entrynode);
    }
    else
    {
        qc_writef (qc, "    transition %i %i", seq->entrynode, seq->exitnode);
    }
}

static void decomp_writeseqmotiontype (FILE *qc, mstudioseqdesc_t *seq)
{
    if (!(seq->motiontype & STUDIO_TYPES))
        return;

    int i;

    qc_write2f (qc, "   ");
    
    for (i = 0; i < 15; ++i)
    {
        if ((seq->motiontype & STUDIO_TYPES) & (1 << i))
        {
            qc_write2f (qc, " %s", mdl_getmotionflag (1 << i));
        }
    }
    qc_putc (qc, '\n');
}

static void decomp_writeseqevents (FILE *mdl, FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->numevents <= 0)
        return;
    
    int i;
    mstudioevent_t event;

    mdl_seek (mdl, seq->eventindex, SEEK_SET);
    
    for (i = 0; i < seq->numevents; ++i)
    {
        mdl_read (mdl, &event, sizeof (event));
        qc_writef (
            qc,
            "    { event %i %i \"%s\" }",
            event.event,
            event.frame,
            event.options);
    }
}

static void decomp_writeseqdesc (
    FILE *mdl,
    FILE *qc,
    studiohdr_t *header,
    mstudioseqdesc_t *seq)
{
    int i;

    qc_putc (qc, '\n');
    qc_putc (qc, '{');
    qc_putc (qc, '\n');
    
    decomp_writeseqblends (qc, seq);
    decomp_writeseqact (qc, seq);
    decomp_writeseqpivots (mdl, qc, seq);
    decomp_writeseqblendtype (qc, seq);
    decomp_writeseqnodes (qc, seq);
    decomp_writeseqmotiontype (qc, seq);
    decomp_writeseqevents (mdl, qc, seq);

    qc_putc (qc, '}');
    qc_putc (qc, '\n');
}

static void decomp_writesequences (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numseq <= 0)
        return;
    
    int i;

    mstudioseqdesc_t seq;

    for (i = 0; i < header->numseq; ++i)
    {
        mdl_seek (mdl, header->seqindex + sizeof(seq) * i, SEEK_SET);
        mdl_read (mdl, &seq, sizeof(seq));

        qc_write2f (qc, "$sequence %s", seq.label);

        fixpath (seq.label, true);

        if (!decomp_simplesequence (&seq))
        {
            decomp_writeseqdesc (mdl, qc, header, &seq);
            continue;
        }

        qc_write2f (qc, " \"anims/%s\" fps %g", seq.label, seq.fps);
        
        if (seq.flags & STUDIO_LOOPING)
            qc_write2f (qc, " loop");
        
        qc_putc (qc, '\n');
    }
}

void decomp_writetextures (FILE *mdl, const char *bmpdir, studiohdr_t *header)
{
    int i;
    mstudiotexture_t texture;
    char *name, *ext;

    for (i = 0; i < header->numtextures; ++i)
    {
        mdl_seek (mdl, header->textureindex + sizeof (texture) * i, SEEK_SET);
        mdl_read (mdl, &texture, sizeof (texture));

        fixpath (texture.name, true);
        stripext (texture.name);
        
        decomp_studiotexture (mdl, bmpdir, header, &texture);
    }
}

void decomp_mdl (
    const char *mdlname,
    const char *qcname,
    const char *cd,
    const char *cdtexture,
    const char *qcdir,
    const char *smddir,
    const char *bmpdir)
{
    FILE *mdl = mdl_open (mdlname);
    FILE *qc = qc_open (qcdir, qcname, "qc");

    studiohdr_t header;
    mdl_read (mdl, &header, sizeof (header));
    fixpath (header.name, false);

    decomp_writeinfo (mdl, qc, cd, cdtexture, &header);
    decomp_writebodygroups (mdl, qc, smddir, &header);
    decomp_writeskingroups (mdl, qc, &header);
    decomp_writeattachments (mdl, qc, &header);
    decomp_writecontrollers (mdl, qc, &header);
    decomp_writehitboxes (mdl, qc, &header);
    decomp_writesequences (mdl, qc, &header);
    decomp_writetextures (mdl, bmpdir, &header);

    fclose (qc);
    fclose (mdl);

    fprintf (stdout, "Done!\n");
}

static int getargs (int argc, char **argv)
{
    if (argc < 2)
    {
print_help:
        fprintf (stdout, "Usage: decompmdl [options...] <input file> [<output file>]\n\n");
        fprintf (stdout, "Options:\n");
        fprintf (stdout, "\t-h --help\t\tDisplay this message and exit\n");
        exit (0);
    }

    int i;

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
            break;
        
        if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--help"))
        {
            goto print_help;
        }
        else
        {
            fprintf (stdout, "Unknown option: \"%s\"\n", argv[i]);
        }
    }

    if (i >= argc)
    {
        error (1, "No input file provided\n");
    }

    return i;
}

static void getdirs (
    char *in,
    char *out,
    char *cd,
    char *cdtexture,
    char **qcdir,
    char **qcname,
    char **smddir,
    char **bmpdir)
{
    if (!out)
    {
        *qcname = strdup (in);
        stripext (*qcname);

        *qcdir = strdup (skippath (*qcname));
    }
    else
    {
        char *name, *ext;
        *qcdir = strdup (out);
        
        filebase (out, &name, &ext);
        
        if (*ext != '\0')
        {
            *qcname = strdup (out);
            stripfilename (*qcdir);
        }
        else
        {
            *qcname = strdup (in);
            stripext (*qcdir);
        }

        stripext (*qcname);
    }

    size_t len;

    len = strlen (*qcdir) + strlen (cd) + 8;

    *smddir = (char *)calloc (len, 1);
    strcpy (*smddir, *qcdir);

    len = strlen (*smddir);

    if ((*smddir)[len - 1] != '/')
        strcat (*smddir, "/");
    
    strcat (*smddir, cd);

    len = strlen (*smddir) + strlen (cdtexture) + 8;

    *bmpdir = (char *)calloc (len, 1);
    strcpy (*bmpdir, *smddir);

    len = strlen (*bmpdir);

    if ((*bmpdir)[len - 1] != '/')
        strcat (*bmpdir, "/");
    
    strcat (*bmpdir, cdtexture);

    /*
    fprintf (
        stdout,
        "\"%s\"\n\"%s\"\n\"%s\"\n\"%s\"\n",
        skippath (*qcname),
        *qcdir,
        *smddir,
        *bmpdir);
    */
}

int main (int argc, char **argv)
{
    int i = getargs (argc, argv);
    
    char *in = argv[i];
    char *out = (i + 1 < argc) ? argv[i + 1] : NULL;

    char *cd = ".";
    char *cdtexture = "./maps_8bit";

    char *qcdir, *qcname, *smddir, *bmpdir;

    getdirs (in, out, cd, cdtexture, &qcdir, &qcname, &smddir, &bmpdir);

    decomp_mdl (in, skippath (qcname), cd, cdtexture, qcdir, smddir, bmpdir);

    free (bmpdir);
    free (smddir);
    free (qcname);
    free (qcdir);

    return 0;
}