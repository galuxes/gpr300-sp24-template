#version 450
out vec4 FragColor; //The color of this fragment

in vec2 UV;

uniform layout(binding = 0) sampler2D _gPositions;
uniform layout(binding = 1) sampler2D _gNormals;
uniform layout(binding = 2) sampler2D _gAlbedo;

vec4 LightSpacePos;
uniform mat4 _LightViewProj;

uniform sampler2D _ShadowMap;
uniform vec3 _EyePos;
uniform vec3 _LightDirection;
uniform vec3 _LightColor = vec3(1.0);
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);
uniform float _minBias = 0.02;
uniform float _maxBias = 0.2;


struct Material{
	float Ka; //Ambient coefficient (0-1)
	float Kd; //Diffuse coefficient (0-1)
	float Ks; //Specular coefficient (0-1)
	float Shininess; //Affects size of specular highlight
};
uniform Material _Material;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float bias){
	//Homogeneous Clip space to NDC [-w,w] to [-1,1]
    vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
    //Convert from [-1,1] to [0,1]
    sampleCoord = sampleCoord * 0.5 + 0.5;
	float myDepth = sampleCoord.z - bias; 

	float totalShadow = 0;

	vec2 texelOffset = 1.0 / textureSize(shadowMap,0);
	for(int y = -1; y <= 1; y++)
	{
		for(int x = -1; x <= 1; x++)
		{
			vec2 _uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
			totalShadow+=step(texture(shadowMap,_uv).r, myDepth);
		}
	}

	return totalShadow/=9.0;
}

vec3 CalcLight(vec3 normal, vec3 worldPos, vec3 albedo)
{
	//Light pointing straight down
	vec3 toLight = -_LightDirection;
	float diffuseFactor = max(dot(normal,toLight),0.0);
	//Calculate specularly reflected light
	vec3 toEye = normalize(_EyePos - worldPos);
	//Blinn-phong uses half angle
	vec3 h = normalize(toLight + toEye);
	float specularFactor = pow(max(dot(normal,h),0.0),_Material.Shininess);
	//Combination of specular and diffuse reflection
	vec3 lightColor = (_Material.Kd * diffuseFactor + _Material.Ks * specularFactor) * _LightColor;
	lightColor+=_AmbientColor * _Material.Ka;

	float bias = max(_maxBias * (1.0 - dot(normal,toLight)),_minBias);
	//1: in shadow, 0: out of shadow
	float shadow = calcShadow(_ShadowMap, LightSpacePos, bias); 
	return lightColor * (1.0 - shadow);
	//light += _AmbientColor * _Material.Ka;
}

void main(){

	vec3 normal = texture(_gNormals,UV).xyz;
	vec3 worldPos = texture(_gPositions,UV).xyz;
	vec3 albedo = texture(_gAlbedo,UV).xyz;
	
	vec3 light = vec3(0);

	LightSpacePos = _LightViewProj * vec4(worldPos, 1);

	light += CalcLight(normal, worldPos, albedo);

	FragColor = vec4(albedo * light,1.0);
}

