#include "PhotonMapRenderer.h"

#include <algorithm>

#include "../../Utility/Math.h"

glm::vec3 PhotonMapRenderer::GetPixelColor(const Ray & ray) {
	return TraceRay(ray);
}

PhotonMapRenderer::PhotonMapRenderer(const Scene & _SCENE, const unsigned int _MAX_DEPTH, const unsigned int _BOUNCES_PER_HIT) :
	SCENE(_SCENE), MAX_DEPTH(_MAX_DEPTH), BOUNCES_PER_HIT(_BOUNCES_PER_HIT), Renderer("Photon Map Renderer") { }

glm::vec3 PhotonMapRenderer::TraceRay(const Ray & ray, const unsigned int DEPTH) {
	unsigned int intersectionPrimitiveIndex, intersectionRenderGroupIndex;
	float intersectionDistance;
	bool intersectionFound = SCENE.RayCast(ray, intersectionRenderGroupIndex, intersectionPrimitiveIndex, intersectionDistance);

	/* If the ray doesn't intersect, simply return (0, 0, 0). */
	if (!intersectionFound) { return glm::vec3(0, 0, 0); }

	/*  Calculate intersection point. */
	glm::vec3 intersectionPoint = ray.from + ray.direction * intersectionDistance;
	RenderGroup renderGroup = SCENE.renderGroups[intersectionRenderGroupIndex];
	// Calculate the direct light at the current pos and return
	// if we should not trace this ray any further.
	float r = (float)std::rand() / (float)RAND_MAX;
	if (DEPTH == MAX_DEPTH || r > 0.75f) {
		return CalculateDirectIlluminationAtPos(intersectionPoint, ray.direction,
												*renderGroup.primitives[intersectionPrimitiveIndex],
												*renderGroup.material);
	}

	assert(depth > 0);
	assert(bouncesPerHit > 0);

	/* Retrieve primitive and material information for the intersected object. */
	const auto & intersectionPrimitive = renderGroup.primitives[intersectionPrimitiveIndex];
	Material* hitMaterial = renderGroup.material;
	if (hitMaterial->IsEmissive()) {
		// We could also add emission color to the end result. Returning it here speeds up rendering.
		const float intersectionRadianceFactor = glm::dot(-ray.direction, intersectionPrimitive->GetNormal(intersectionPoint));
		return intersectionRadianceFactor * renderGroup.material->GetEmissionColor();
	}

	/* Calculate normal. */
	glm::vec3 hitNormal = intersectionPrimitive->GetNormal(intersectionPoint);

	/* Shoot rays and integrate based on BRDF sampling. */
	glm::vec3 colorAccumulator = { 0,0,0 };
	// Only cast against light sources for direct light

	//for (unsigned int i = 0; i < emissiveRenderGroups.size(); ++i) {
	for (unsigned int i = 0; i < BOUNCES_PER_HIT; ++i) {
		//glm::vec3 surfPos = emissiveRenderGroups[i]->GetRandomPositionOnSurface();//GetCenter();// 
		glm::vec3 reflectionDirection = Math::RandomHemishpereSampleDirection(hitNormal);
		//glm::normalize(surfPos - intersectionPoint);// = Math::RandomHemishpereSampleDirection(hitNormal);
		/*std::vector<Photon const*> closestPhotons;
		std::vector<Photon const*> photons = photonMap->GetIndirectPhotonsInOctreeNodeOfPosition(intersectionPoint);
		photonMap->GetNClosestPhotonsInOctreeNodeOfPosition(photons, intersectionPoint, 1, closestPhotons);
		photons = photonMap->GetDirectPhotonsInOctreeNodeOfPosition(intersectionPoint);
		photonMap->GetNClosestPhotonsInOctreeNodeOfPosition(photons, intersectionPoint, 1, closestPhotons);
		for (unsigned int pIdx = 0; pIdx < closestPhotons.size(); ++pIdx) {
		glm::vec3 reflectionDirection = -closestPhotons[pIdx]->direction;*/
		assert(dot(reflectionDirection, hitNormal) > -FLT_EPSILON);
		Ray reflectedRay(intersectionPoint, reflectionDirection);
		const glm::vec3 incomingRadiance = TraceRay(reflectedRay, DEPTH + 1);
		colorAccumulator += hitMaterial->CalculateDiffuseLighting(-reflectedRay.direction, -ray.direction, hitNormal, incomingRadiance);
		//}	
	}
	// Add indirect light from photons
	/*const auto & allIndirPhotons = photonMap->GetIndirectPhotonsInOctreeNodeOfPosition(intersectionPoint);
	std::vector<Photon const*> closestIndirPhotons;
	photonMap->GetNClosestPhotonsInOctreeNodeOfPosition(allIndirPhotons, intersectionPoint, 10, closestIndirPhotons);
	float photonDistance;
	for (const Photon * ip : closestIndirPhotons) {
	photonDistance = glm::distance(intersectionPoint, ip->position);
	colorAccumulator +=  0.001f*ip->color / photonDistance;
	}*/
	return (1.0f / (float)BOUNCES_PER_HIT) *colorAccumulator;
}

glm::vec3 PhotonMapRenderer::CalculateDirectIlluminationAtPos(const glm::vec3 & pos, const glm::vec3 & incomingDirection, const Primitive & prim, const Material & material) const {
	glm::vec3 colorAccumulator = { 0,0,0 };

	Octree::OctreeNode* node = photonMap->GetOctreeNodeOfPosition(pos);
	std::vector<Photon const*> directPhotons = node->directPhotons;
	std::vector<Photon const*> indirectPhotons = node->indirectPhotons;
	std::vector<Photon const*> shadowPhotons = node->shadowPhotons;
	// If there are no shadow photons and several directPhotons then we approximate light with photons
	if (shadowPhotons.size() == 0 && directPhotons.size() > photonMap->minPhotonsPerNode) {
		std::vector<Photon const*> photonsWithinRadius;
		glm::vec3 corner = node->axisAlignedBoundingBox.maximum;
		float radius = glm::distance(node->axisAlignedBoundingBox.GetCenter(), corner);
		photonMap->GetPhotonsInOctreeNodeOfPositionWithinRadius(directPhotons, pos, radius, photonsWithinRadius);
		for (Photon const* dp : directPhotons) {
			float distance = glm::distance(pos, dp->position);
			float weight = std::max(0.0f, 1.0f - distance / radius);
			colorAccumulator += weight * dp->color;
		}
	}
	// If there are no direct photons we assume its not lit by any light
	if (directPhotons.size()) {
		return glm::vec3(0, 0, 0);
	}

	Ray ray;
	ray.from = pos + 0.01f * prim.GetNormal(pos); // + 0.01 normal to not cast from inside the object	
	unsigned int intersectionRenderGroupIdx;
	unsigned int intersectionPrimitiveIdx;
	float intersectionDistance;
	// Check all light sources
	for (RenderGroup* lightSource : SCENE.emissiveRenderGroups) {
		glm::vec3 lightSurfPos = lightSource->GetRandomPositionOnSurface();
		ray.direction = glm::normalize(lightSurfPos - ray.from);
		// Cast ray towards light source
		if (SCENE.RayCast(ray, intersectionRenderGroupIdx, intersectionPrimitiveIdx, intersectionDistance, false)) {
			// Only add color if we did hit the light source we casted towards
			const auto & renderGroup = SCENE.renderGroups[intersectionRenderGroupIdx];
			if (&renderGroup == lightSource) {
				Primitive* lightPrim = renderGroup.primitives[intersectionPrimitiveIdx];
				glm::vec3 lightNormal = lightPrim->GetNormal(ray.from + intersectionDistance * ray.direction);
				const float intersectionRadianceFactor = glm::dot(-ray.direction, lightNormal);
				colorAccumulator += material.CalculateDiffuseLighting(-ray.direction, incomingDirection,
																	  prim.GetNormal(ray.from),
																	  lightSource->material->GetEmissionColor() * intersectionRadianceFactor);
			}
		}
	}

	return (1.0f / (float)SCENE.emissiveRenderGroups.size()) * colorAccumulator;
}