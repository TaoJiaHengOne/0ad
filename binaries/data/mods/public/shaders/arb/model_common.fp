!!ARBfp1.0
#ifdef USE_FP_SHADOW
  OPTION ARB_fragment_program_shadow;
#endif

#ifdef LIGHTING_MODEL_old
  #define CLAMP_LIGHTING
#endif

#ifdef CLAMP_LIGHTING // for compat with old scenarios that expect clamped lighting
  #define MAD_MAYBE_SAT MAD_SAT
#else
  #define MAD_MAYBE_SAT MAD
#endif

#ifdef USE_OBJECTCOLOR
  PARAM objectColor = program.local[0];
#else
#ifdef USE_PLAYERCOLOR
  PARAM playerColor = program.local[0];
#endif
#endif

PARAM shadingColor = program.local[1];
PARAM ambient = program.local[2];

#ifdef USE_SHADOW_PCF
  PARAM shadowOffsets1 = program.local[3];
  PARAM shadowOffsets2 = program.local[4];
  TEMP offset;
#endif

TEMP tex;
TEMP temp;
TEMP diffuse;
TEMP color;

TEX tex, fragment.texcoord[0], texture[0], 2D;
#ifdef USE_TRANSPARENT
  MOV result.color.a, tex;
#endif

// Apply coloring based on texture alpha
#ifdef USE_OBJECTCOLOR
  LRP temp.rgb, objectColor, 1.0, tex.a;
  MUL color.rgb, tex, temp;
#else
#ifdef USE_PLAYERCOLOR
  LRP temp.rgb, playerColor, 1.0, tex.a;
  MUL color.rgb, tex, temp;
#else
  MOV color.rgb, tex;
#endif
#endif

// Compute color = texture * (ambient + diffuse*shadow)
// (diffuse is 2*fragment.color due to clamp-avoidance in the vertex program)
#ifdef USE_SHADOW
  #ifdef USE_FP_SHADOW
    #ifdef USE_SHADOW_PCF
      MOV offset.zw, fragment.texcoord[1];
      ADD offset.xy, fragment.texcoord[1], shadowOffsets1;
      TEX temp.x, offset, texture[1], SHADOW2D;
      ADD offset.xy, fragment.texcoord[1], shadowOffsets1.zwzw;
      TEX temp.y, offset, texture[1], SHADOW2D;
      ADD offset.xy, fragment.texcoord[1], shadowOffsets2;
      TEX temp.z, offset, texture[1], SHADOW2D;
      ADD offset.xy, fragment.texcoord[1], shadowOffsets2.zwzw;
      TEX temp.w, offset, texture[1], SHADOW2D;
      DP4 temp, temp, 0.25;
    #else
      TEX temp, fragment.texcoord[1], texture[1], SHADOW2D;
    #endif
  #else
    TEX tex, fragment.texcoord[1], texture[1], 2D;
    MOV_SAT temp.z, fragment.texcoord[1].z;
    SGE temp, tex.x, temp.z;
  #endif
  #ifdef CLAMP_LIGHTING
    MAD_SAT diffuse.rgb, fragment.color, 2.0, ambient;
    LRP temp.rgb, temp, diffuse, ambient;
  #else
    MUL diffuse.rgb, fragment.color, 2.0;
    MAD temp.rgb, diffuse, temp, ambient;
  #endif
  MUL color.rgb, color, temp;
#else
  MAD_MAYBE_SAT temp.rgb, fragment.color, 2.0, ambient;
  MUL color.rgb, color, temp;
#endif

// Multiply everything by the LOS texture
TEX tex.a, fragment.texcoord[2], texture[2], 2D;
MUL color.rgb, color, tex.a;

MUL result.color.rgb, color, shadingColor;

END
