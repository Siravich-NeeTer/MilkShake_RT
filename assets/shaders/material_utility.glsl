vec3 ComputeDiffuse(vec3 lightDir, vec3 normal)
{
	// TODO: Need to plug this to material component
	vec3 diffuseConstant = vec3(1.0f);

	float dotNL = max(dot(normal, lightDir), 0.0f);
	vec3 c = diffuseConstant * dotNL;

	return c;
}
vec3 ComputeSpecular(vec3 viewDir, vec3 lightDir, vec3 normal) 
{
	float shininessConstant = 0.25f;
	float specularConstant = 0.1f;

	// Compute specular only if not in shadow
	const float kPi = 3.14159265;
	const float kShininess = max(shininessConstant, 4.0);
	
	// Specular
	const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);
	vec3        V = normalize(-viewDir);
	vec3        R = reflect(-lightDir, normal);
	float       specular = kEnergyConservation * pow(max(dot(V, R), 0.0), kShininess);

	return vec3(specularConstant * specular);
}