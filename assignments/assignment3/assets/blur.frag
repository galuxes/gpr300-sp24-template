#version 450
out vec4 FragColor; //The color of this fragment

mat3 GAUSSIANBLUR = mat3(1,2,1,2,4,2,1,2,1) * .0625f;
mat3 GAUSSIANBLUR2 = mat3(0.75,1.5,0.75,1.5,7,1.5,0.75,1.5,0.75) * .0625f;
mat3 EDGEDETECTION = mat3( -1, -1, -1, -1, 8, -1, -1, -1, -1);
vec2[] OFFSETS = vec2[](vec2(-1,1), vec2(0,1), vec2(1,1),vec2(-1,0), vec2(0,0), vec2(1,0), vec2(-1,-1),vec2(0,-1),vec2(1,-1));

in vec2 uv;
uniform sampler2D _MainTex; 

void main(){


	vec2 offsetDistance = vec2((1));
    offsetDistance *= 1.0 / textureSize(_MainTex,0).xy;


    vec3 edgeColor = vec3(0.0);
    
    for(int i = 0; i < 9; i++){
        //Sample from a neighboring pixel
        vec3 color = texture(_MainTex, uv + OFFSETS[i] * offsetDistance).rgb;
        
        //Convert index i to kernel col,row
        int col = i % 3;
        int row = i / 3;
        
        //Multiply current sample by kernel weight
        color*=EDGEDETECTION[col][row];
        
        //Accumulate
        edgeColor+=color;
    }

    offsetDistance = vec2((3));
    offsetDistance *= 1.0 / textureSize(_MainTex,0).xy;

	vec3 blurColor = vec3(0.0);
    
    for(int i = 0; i < 9; i++){
        //Sample from a neighboring pixel
        vec3 color = texture(_MainTex, uv + OFFSETS[i] * offsetDistance).rgb;
        
        //Convert index i to kernel col,row
        int col = i % 3;
        int row = i / 3;
        
        //Multiply current sample by kernel weight
        color*=GAUSSIANBLUR[col][row];
        
        //Accumulate
        blurColor+=color;
    }

    offsetDistance = vec2((1));
    offsetDistance *= 1.0 / textureSize(_MainTex,0).xy;

    vec3 blurColor2 = vec3(0.0);
    
    for(int i = 0; i < 9; i++){
        //Sample from a neighboring pixel
        vec3 color = texture(_MainTex, uv + OFFSETS[i] * offsetDistance).rgb;
        
        //Convert index i to kernel col,row
        int col = i % 3;
        int row = i / 3;
        
        //Multiply current sample by kernel weight
        color*=GAUSSIANBLUR2[col][row];
        
        //Accumulate
        blurColor2+=color;
    }

    //blurColor = blurColor2 - blurColor;

	FragColor = vec4(texture(_MainTex, uv).rgb,1.0);
}