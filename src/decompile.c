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
    FILE *tex,
    const char *smddir,
    studiohdr_t *header,
    studiohdr_t *textureheader,
    mstudiomodel_t *model,
    const char *nodes);

void decomp_studiotexture (
    FILE *tex,
    const char *bmpdir,
    mstudiotexture_t *texture);

void decomp_studioanim (
    FILE *mdl,
    FILE *smd,
    studiohdr_t *header,
    mstudiobone_t *bones,
    int numframes,
    int animindex,
    const char *nodes);

static void decomp_writeinfo (
    FILE *mdl,
    FILE *tex,
    FILE *qc,
    const char *cd,
    const char *cdtexture,
    studiohdr_t *header,
    studiohdr_t *textureheader)
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

    int i;
    mstudiotexture_t texture;

    mdl_seek (tex, textureheader->textureindex, SEEK_SET);
    for (i = 0; i < textureheader->numtextures; ++i)
    {
        mdl_read (tex, &texture, sizeof (texture));

        if (!(texture.flags & ~(STUDIO_NF_CHROME | STUDIO_NF_FLATSHADE)))
            continue;

        fixpath (texture.name, true);
        stripext (texture.name);
        
        if (texture.flags & STUDIO_NF_ADDITIVE)
        {
            qc_writef (qc, "$texrendermode %s.bmp additive", texture.name);
        }
        if (texture.flags & STUDIO_NF_MASKED)
        {
            qc_writef (qc, "$texrendermode %s.bmp masked", texture.name);
        }
        /* Toodles: Sven Co-op & Xash3D support this. */
        if (texture.flags & STUDIO_NF_FULLBRIGHT)
        {
            qc_writef (qc, "$texrendermode %s.bmp fullbright", texture.name);
        }
    }
}

static char *decomp_makenodes (FILE *mdl, studiohdr_t *header)
{
    char line[45];
    mstudiobone_t bone;
    
    char *str = (char *)memalloc (10 + 45 * header->numbones, 1);

    mdl_seek (mdl, header->boneindex, SEEK_SET);
    strcat (str, "nodes\n");

    int i;
    for (i = 0; i < header->numbones; ++i)
    {
        mdl_read (mdl, &bone, sizeof (bone));
        sprintf (line, "  %i \"%s\" %i\n", i, bone.name, bone.parent);
        strcat (str, line);
    }

    strcat (str, "end");

    return str;
}

static void decomp_writebodygroups (
    FILE *mdl,
    FILE *tex,
    FILE *qc,
    const char *smddir,
    studiohdr_t *header,
    studiohdr_t *textureheader,
    const char *nodes)
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

            decomp_studiomodel (mdl, tex, smddir, header, textureheader, &model, nodes);
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
    FILE *tex,
    FILE *qc,
    studiohdr_t *textureheader,
    short** out_skin_index,
    bool** out_skin)
{
    int i, j;

    int families = textureheader->numskinfamilies;
    int refs = textureheader->numskinref;
    bool any_skins = false;

    bool *skin = (bool *)memalloc (refs, sizeof (*skin));

    short *skin_index = (short *)memalloc (families * refs, sizeof (*skin_index));

    mdl_seek (tex, textureheader->skinindex, SEEK_SET);
    mdl_read (tex, skin_index, families * refs * sizeof (*skin_index));

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

static void decomp_writeskingroups (
    FILE *tex,
    FILE *qc,
    studiohdr_t *textureheader)
{
    short* skin_index;
    bool* skin;

    if (!decomp_buildskingroups (tex, qc, textureheader, &skin_index, &skin))
        goto skins_done;

    qc_write (qc, "$texturegroup group");
    qc_putc (qc, '{');
    qc_putc (qc, '\n');

    int i, j;

    char texture_name[64];

    for (i = 0; i < textureheader->numskinref; ++i)
    {
        qc_write2f (qc, "    { ");

        for (j = 0; j < textureheader->numskinfamilies; ++j)
        {
            if (!skin[j])
                continue;
            
            mdl_seek (
                tex,
                textureheader->textureindex
                    + sizeof (mstudiotexture_t)
                    * skin_index[i * textureheader->numskinref + j],
                SEEK_SET);
            
            mdl_read (tex, &texture_name, sizeof (texture_name));
            
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

static void decomp_writeseqblends (FILE *qc, const char *cdanim, mstudioseqdesc_t *seq)
{
    if (seq->numblends <= 1)
    {
        qc_writef (qc, "    \"%s/%s\"", cdanim, seq->label);
    }
    else if (seq->numblends == 2)
    {
        /* I'm certain this is always the case with official models. */
        qc_writef (qc, "    \"%s/%s_down\"", cdanim, seq->label);
        qc_writef (qc, "    \"%s/%s_up\"", cdanim, seq->label);
    }
    else
    {
        int i;
        for (i = 0; i < seq->numblends; ++i)
        {
            qc_writef (qc, "    \"%s/%s_blend_%.02i\"", cdanim, seq->label, i);
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
    const char *cdanim,
    studiohdr_t *header,
    mstudioseqdesc_t *seq)
{
    int i;

    qc_putc (qc, '\n');
    qc_putc (qc, '{');
    qc_putc (qc, '\n');
    
    decomp_writeseqblends (qc, cdanim, seq);
    decomp_writeseqact (qc, seq);
    decomp_writeseqpivots (mdl, qc, seq);
    decomp_writeseqblendtype (qc, seq);
    decomp_writeseqnodes (qc, seq);
    decomp_writeseqmotiontype (qc, seq);
    decomp_writeseqevents (mdl, qc, seq);

    qc_putc (qc, '}');
    qc_putc (qc, '\n');
}

static void decomp_writeanimations (
    FILE *mdl,
    const char *animdir,
    studiohdr_t *header,
    const char *nodes,
    mstudioseqdesc_t *seq,
    mstudiobone_t *bones)
{
    mstudioseqgroup_t group;
    char *animname = (char *)memalloc (strlen (seq->label) + 16, 1);
    char blendnum[4];
    FILE *smd;

    mdl_seek (mdl, header->seqgroupindex + sizeof (group) * seq->seqgroup, SEEK_SET);
    mdl_read (mdl, &group, sizeof (group));

    /* Toodles TODO: Support external animation groups. */
    int animindex = group.unused2 + seq->animindex;
    int i;

    for (i = 0; i < seq->numblends; ++i)
    {
        strcpy (animname, seq->label);

        if (seq->numblends == 2)
        {
            strcat (animname, i == 0 ? "down" : "up");
        }
        else if (seq->numblends > 2)
        {
            strcat (animname, "_blend_");
            snprintf (blendnum, 8, "%.02i", i);
            strcat (animname, blendnum);
        }

        smd = qc_open (animdir, animname, "smd");

        decomp_studioanim (
            mdl,
            smd,
            header,
            bones,
            seq->numframes,
            animindex + sizeof (mstudioanim_t) * i,
            nodes);

        fclose (smd);
        fprintf (stdout, "Done!\n");
    }

    free (animname);
}

static void decomp_writesequences (
    FILE *mdl,
    FILE *qc,
    const char *smddir,
    const char *cdanim,
    studiohdr_t *header,
    const char *nodes)
{
    if (header->numseq <= 0)
        return;
    
    int i, j;

    mstudioseqdesc_t seq;
    mstudioseqgroup_t group;
    int animindex;

    char *animdir = appenddir (smddir, cdanim);
    mstudiobone_t *bones = (mstudiobone_t *)memalloc (header->numbones, sizeof (*bones));

    mdl_seek (mdl, header->boneindex, SEEK_SET);
    mdl_read (mdl, bones, header->numbones * sizeof (*bones));

    for (i = 0; i < header->numseq; ++i)
    {
        mdl_seek (mdl, header->seqindex + sizeof(seq) * i, SEEK_SET);
        mdl_read (mdl, &seq, sizeof(seq));

        qc_write2f (qc, "$sequence %s", seq.label);

        fixpath (seq.label, true);

        decomp_writeanimations (mdl, animdir, header, nodes, &seq, bones);

        if (!decomp_simplesequence (&seq))
        {
            decomp_writeseqdesc (mdl, qc, cdanim, header, &seq);
            continue;
        }

        qc_write2f (qc, " \"%s/%s\" fps %g", cdanim, seq.label, seq.fps);
        
        if (seq.flags & STUDIO_LOOPING)
            qc_write2f (qc, " loop");
        
        qc_putc (qc, '\n');
    }

    free (animdir);
    free (bones);
}

void decomp_writetextures (
    FILE *tex,
    const char *smddir,
    const char *cdtexture,
    studiohdr_t *textureheader)
{
    int i;
    mstudiotexture_t texture;
    char *name, *ext;

    char *bmpdir = appenddir (smddir, cdtexture);

    for (i = 0; i < textureheader->numtextures; ++i)
    {
        mdl_seek (tex, textureheader->textureindex + sizeof (texture) * i, SEEK_SET);
        mdl_read (tex, &texture, sizeof (texture));

        fixpath (texture.name, true);
        stripext (texture.name);
        
        decomp_studiotexture (tex, bmpdir, &texture);
    }

    free (bmpdir);
}

void decomp_mdl (
    const char *mdlname,
    const char *qcname,
    const char *cd,
    const char *cdtexture,
    const char *cdanim,
    const char *qcdir,
    const char *smddir)
{
    FILE *mdl = mdl_open (mdlname, false);
    FILE *qc = qc_open (qcdir, qcname, "qc");

    studiohdr_t header;
    mdl_read (mdl, &header, sizeof (header));
    fixpath (header.name, false);

    /* Init the texture vars with the regular model info. */
    FILE *tex = mdl;
    studiohdr_t textureheader;
    memcpy (&textureheader, &header, sizeof (textureheader));

    /* Init the texture model if necessary. */
    if (header.numtextures == 0)
    {
        char *texname = (char *)memalloc (strlen (mdlname) + 2, 1);
        char *name, *ext;

        filebase (mdlname, &name, &ext);
        
        strcpy (texname, mdlname);
        stripext (texname);
        strcat (texname, "t");
        strcat (texname, ext);

#ifndef WIN32
        tex = mdl_open (texname, true);
        
        if (!tex)
        {
            texname[strlen (mdlname) - strlen (ext)] = '\0';
            strcat (texname, "T");
            strcat (texname, ext);

            tex = mdl_open (texname, false);
        }
#else
        tex = mdl_open (texname, false);
#endif

        free (texname);

        mdl_read (tex, &textureheader, sizeof (textureheader));
    }

    char *nodes = decomp_makenodes (mdl, &header);

    decomp_writeinfo (mdl, tex, qc, cd, cdtexture, &header, &textureheader);
    decomp_writebodygroups (mdl, tex, qc, smddir, &header, &textureheader, nodes);
    decomp_writeskingroups (tex, qc, &textureheader);
    decomp_writeattachments (mdl, qc, &header);
    decomp_writecontrollers (mdl, qc, &header);
    decomp_writehitboxes (mdl, qc, &header);
    decomp_writesequences (mdl, qc, smddir, cdanim, &header, nodes);
    decomp_writetextures (tex, smddir, cdtexture, &textureheader);

    free (nodes);

    if (tex != mdl)
    {
        fclose (tex);
        fprintf (stdout, "Done!\n");
    }
    
    fclose (qc);
    fclose (mdl);

    fprintf (stdout, "Done!\n");
}

static int getargs (
    int argc,
    char **argv,
    char **cd,
    bool *havecd,
    char **cdtexture,
    char **cdanim)
{
    if (argc < 2)
    {
print_help:
        fprintf (stdout, "Usage: decompmdl [options...] <input file> [<output file>]\n\n");
        fprintf (stdout, "Options:\n");
        fprintf (stdout, "\t-help\t\t\tDisplay this message and exit.\n\n");
        fprintf (stdout, "\t-cd <path>\t\tSets the data path. Default: \".\"\n\t\t\t\tIf set, data will be placed relative to root path.\n\n");
        fprintf (stdout, "\t-cdtexture <path>\tSets the texture path, relative to data path. Default: \"./maps_8bit\"\n\n");
        fprintf (stdout, "\t-cdanim <path>\t\tSets the animation path, relative to data path. Default: \"./anims\"\n\n");
        exit (0);
    }

    int i;

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
            break;
        
        if (!strcmp (argv[i], "-help"))
        {
            goto print_help;
        }
        else if (!strcmp (argv[i], "-cd"))
        {
            *cd = argv[i + 1];
            *havecd = true;
            fprintf (stdout, "Data path set to: \"%s\"\n", *cd);
            ++i;
        }
        else if (!strcmp (argv[i], "-cdtexture"))
        {
            *cdtexture = argv[i + 1];
            fprintf (stdout, "Texture path set to: \"%s\"\n", *cdtexture);
            ++i;
        }
        else if (!strcmp (argv[i], "-cdanim"))
        {
            *cdanim = argv[i + 1];
            fprintf (stdout, "Animation path set to: \"%s\"\n", *cdanim);
            ++i;
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
    char **qcdir,
    char **qcname,
    bool havecd)
{
    if (havecd)
    {
        *qcdir = ".";
    }

    if (!out) /* Put files in sub directory. */
    {
        *qcname = strdup (skippath (in));
        stripext (*qcname);

        if (!havecd)
        {
            *qcdir = strdup (*qcname);
        }
        return;
    }

    char *name, *ext;
    
    filebase (out, &name, &ext);
    
    if (*ext) /* QC name provided. Put files in root directory. */
    {
        *qcname = strdup (out);
        stripext (*qcname);

        if (!havecd)
        {
            *qcdir = strdup (out);
            stripfilename (*qcdir);
        }
    }
    else /* Put files in sub directory. */
    {
        *qcname = strdup (skippath (in));
        stripext (*qcname);

        if (!havecd)
        {
            *qcdir = appenddir (out, *qcname);
        }
    }
}

int main (int argc, char **argv)
{
    char *cd = ".";
    bool havecd = false;
    char *cdtexture = "./maps_8bit";
    char *cdanim = "./anims";

    int i = getargs (argc, argv, &cd, &havecd, &cdtexture, &cdanim);
    
    char *in = argv[i];
    char *out = (i + 1 < argc) ? argv[i + 1] : NULL;

    char *qcdir, *qcname;

    getdirs (in, out, &qcdir, &qcname, havecd);

    char *smddir = appenddir (qcdir, cd);

    decomp_mdl (in, skippath (qcname), cd, cdtexture, cdanim, qcdir, smddir);

    free (smddir);
    free (qcname);
    
    if (!havecd)
    {
        free (qcdir);
    }

    return 0;
}