//geometryPass.frag 
#version 450 core
layout(location = 0) out vec3 gPosition; //Worldspace position
layout(location = 1) out vec3 gNormal; //Worldspace normal 
layout(location = 2) out vec3 gAlbedo;

in Surface{
	vec3 WorldPos; 
	vec2 TexCoord;
	vec3 WorldNormal;
}fs




void ma

ion = fs_in.WorldPos;
	gAlbedo = texture(_MainTex,fs_in.TexCoord).rgb;
	gNormal = normalize(fs_in.WorldNormal);
}
