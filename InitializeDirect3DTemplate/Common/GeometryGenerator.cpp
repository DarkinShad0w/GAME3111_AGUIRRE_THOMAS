//***************************************************************************************
// GeometryGenerator.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "GeometryGenerator.h"
#include <algorithm>

using namespace DirectX;

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth, uint32 numSubdivisions)
{
    MeshData meshData;

    //
	// Create the vertices.
	//

	Vertex v[24];

	float w2 = 0.5f*width;
	float h2 = 0.5f*height;
	float d2 = 0.5f*depth;
    
	// Fill in the front face vertex data.
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	v[8]  = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9]  = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	meshData.Vertices.assign(&v[0], &v[24]);
 
	//
	// Create the indices.
	//

	uint32 i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7]  = 5; i[8]  = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] =  9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign(&i[0], &i[36]);

    // Put a cap on the number of subdivisions.
    numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

    for(uint32 i = 0; i < numSubdivisions; ++i)
        Subdivide(meshData);

    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
{
    MeshData meshData;

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	meshData.Vertices.push_back( topVertex );

	float phiStep   = XM_PI/stackCount;
	float thetaStep = 2.0f*XM_PI/sliceCount;

	// Compute vertices for each stack ring (do not count the poles as rings).
	for(uint32 i = 1; i <= stackCount-1; ++i)
	{
		float phi = i*phiStep;

		// Vertices of ring.
        for(uint32 j = 0; j <= sliceCount; ++j)
		{
			float theta = j*thetaStep;

			Vertex v;

			// spherical to cartesian
			v.Position.x = radius*sinf(phi)*cosf(theta);
			v.Position.y = radius*cosf(phi);
			v.Position.z = radius*sinf(phi)*sinf(theta);

			// Partial derivative of P with respect to theta
			v.TangentU.x = -radius*sinf(phi)*sinf(theta);
			v.TangentU.y = 0.0f;
			v.TangentU.z = +radius*sinf(phi)*cosf(theta);

			XMVECTOR T = XMLoadFloat3(&v.TangentU);
			XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TexC.x = theta / XM_2PI;
			v.TexC.y = phi / XM_PI;

			meshData.Vertices.push_back( v );
		}
	}

	meshData.Vertices.push_back( bottomVertex );

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

    for(uint32 i = 1; i <= sliceCount; ++i)
	{
		meshData.Indices32.push_back(0);
		meshData.Indices32.push_back(i+1);
		meshData.Indices32.push_back(i);
	}
	
	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
    uint32 baseIndex = 1;
    uint32 ringVertexCount = sliceCount + 1;
	for(uint32 i = 0; i < stackCount-2; ++i)
	{
		for(uint32 j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j+1);
			meshData.Indices32.push_back(baseIndex + (i+1)*ringVertexCount + j);

			meshData.Indices32.push_back(baseIndex + (i+1)*ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j+1);
			meshData.Indices32.push_back(baseIndex + (i+1)*ringVertexCount + j+1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint32 southPoleIndex = (uint32)meshData.Vertices.size()-1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;
	
	for(uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(southPoleIndex);
		meshData.Indices32.push_back(baseIndex+i);
		meshData.Indices32.push_back(baseIndex+i+1);
	}

    return meshData;
}
 
void GeometryGenerator::Subdivide(MeshData& meshData)
{
	// Save a copy of the input geometry.
	MeshData inputCopy = meshData;


	meshData.Vertices.resize(0);
	meshData.Indices32.resize(0);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	uint32 numTris = (uint32)inputCopy.Indices32.size()/3;
	for(uint32 i = 0; i < numTris; ++i)
	{
		Vertex v0 = inputCopy.Vertices[ inputCopy.Indices32[i*3+0] ];
		Vertex v1 = inputCopy.Vertices[ inputCopy.Indices32[i*3+1] ];
		Vertex v2 = inputCopy.Vertices[ inputCopy.Indices32[i*3+2] ];

		//
		// Generate the midpoints.
		//

        Vertex m0 = MidPoint(v0, v1);
        Vertex m1 = MidPoint(v1, v2);
        Vertex m2 = MidPoint(v0, v2);

		//
		// Add new geometry.
		//

		meshData.Vertices.push_back(v0); // 0
		meshData.Vertices.push_back(v1); // 1
		meshData.Vertices.push_back(v2); // 2
		meshData.Vertices.push_back(m0); // 3
		meshData.Vertices.push_back(m1); // 4
		meshData.Vertices.push_back(m2); // 5
 
		meshData.Indices32.push_back(i*6+0);
		meshData.Indices32.push_back(i*6+3);
		meshData.Indices32.push_back(i*6+5);

		meshData.Indices32.push_back(i*6+3);
		meshData.Indices32.push_back(i*6+4);
		meshData.Indices32.push_back(i*6+5);

		meshData.Indices32.push_back(i*6+5);
		meshData.Indices32.push_back(i*6+4);
		meshData.Indices32.push_back(i*6+2);

		meshData.Indices32.push_back(i*6+3);
		meshData.Indices32.push_back(i*6+1);
		meshData.Indices32.push_back(i*6+4);
	}
}

GeometryGenerator::Vertex GeometryGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
{
    XMVECTOR p0 = XMLoadFloat3(&v0.Position);
    XMVECTOR p1 = XMLoadFloat3(&v1.Position);

    XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
    XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

    XMVECTOR tan0 = XMLoadFloat3(&v0.TangentU);
    XMVECTOR tan1 = XMLoadFloat3(&v1.TangentU);

    XMVECTOR tex0 = XMLoadFloat2(&v0.TexC);
    XMVECTOR tex1 = XMLoadFloat2(&v1.TexC);

    // Compute the midpoints of all the attributes.  Vectors need to be normalized
    // since linear interpolating can make them not unit length.  
    XMVECTOR pos = 0.5f*(p0 + p1);
    XMVECTOR normal = XMVector3Normalize(0.5f*(n0 + n1));
    XMVECTOR tangent = XMVector3Normalize(0.5f*(tan0+tan1));
    XMVECTOR tex = 0.5f*(tex0 + tex1);

    Vertex v;
    XMStoreFloat3(&v.Position, pos);
    XMStoreFloat3(&v.Normal, normal);
    XMStoreFloat3(&v.TangentU, tangent);
    XMStoreFloat2(&v.TexC, tex);

    return v;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGeosphere(float radius, uint32 numSubdivisions)
{
    MeshData meshData;

	// Put a cap on the number of subdivisions.
    numSubdivisions = std::min<uint32>(numSubdivisions, 6u);

	// Approximate a sphere by tessellating an icosahedron.

	const float X = 0.525731f; 
	const float Z = 0.850651f;

	XMFLOAT3 pos[12] = 
	{
		XMFLOAT3(-X, 0.0f, Z),  XMFLOAT3(X, 0.0f, Z),  
		XMFLOAT3(-X, 0.0f, -Z), XMFLOAT3(X, 0.0f, -Z),    
		XMFLOAT3(0.0f, Z, X),   XMFLOAT3(0.0f, Z, -X), 
		XMFLOAT3(0.0f, -Z, X),  XMFLOAT3(0.0f, -Z, -X),    
		XMFLOAT3(Z, X, 0.0f),   XMFLOAT3(-Z, X, 0.0f), 
		XMFLOAT3(Z, -X, 0.0f),  XMFLOAT3(-Z, -X, 0.0f)
	};

    uint32 k[60] =
	{
		1,4,0,  4,9,0,  4,5,9,  8,5,4,  1,8,4,    
		1,10,8, 10,3,8, 8,3,5,  3,2,5,  3,7,2,    
		3,10,7, 10,6,7, 6,11,7, 6,0,11, 6,1,0, 
		10,1,6, 11,0,9, 2,11,9, 5,2,9,  11,2,7 
	};

    meshData.Vertices.resize(12);
    meshData.Indices32.assign(&k[0], &k[60]);

	for(uint32 i = 0; i < 12; ++i)
		meshData.Vertices[i].Position = pos[i];

	for(uint32 i = 0; i < numSubdivisions; ++i)
		Subdivide(meshData);

	// Project vertices onto sphere and scale.
	for(uint32 i = 0; i < meshData.Vertices.size(); ++i)
	{
		// Project onto unit sphere.
		XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&meshData.Vertices[i].Position));

		// Project onto sphere.
		XMVECTOR p = radius*n;

		XMStoreFloat3(&meshData.Vertices[i].Position, p);
		XMStoreFloat3(&meshData.Vertices[i].Normal, n);

		// Derive texture coordinates from spherical coordinates.
        float theta = atan2f(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

        // Put in [0, 2pi].
        if(theta < 0.0f)
            theta += XM_2PI;

		float phi = acosf(meshData.Vertices[i].Position.y / radius);

		meshData.Vertices[i].TexC.x = theta/XM_2PI;
		meshData.Vertices[i].TexC.y = phi/XM_PI;

		// Partial derivative of P with respect to theta
		meshData.Vertices[i].TangentU.x = -radius*sinf(phi)*sinf(theta);
		meshData.Vertices[i].TangentU.y = 0.0f;
		meshData.Vertices[i].TangentU.z = +radius*sinf(phi)*cosf(theta);

		XMVECTOR T = XMLoadFloat3(&meshData.Vertices[i].TangentU);
		XMStoreFloat3(&meshData.Vertices[i].TangentU, XMVector3Normalize(T));
	}

    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
{
    MeshData meshData;

	//
	// Build Stacks.
	// 

	float stackHeight = height / stackCount;

	// Amount to increment radius as we move up each stack level from bottom to top.
	float radiusStep = (topRadius - bottomRadius) / stackCount;

	uint32 ringCount = stackCount+1;

	// Compute vertices for each stack ring starting at the bottom and moving up.
	for(uint32 i = 0; i < ringCount; ++i)
	{
		float y = -0.5f*height + i*stackHeight;
		float r = bottomRadius + i*radiusStep;

		// vertices of ring
		float dTheta = 2.0f*XM_PI/sliceCount;
		for(uint32 j = 0; j <= sliceCount; ++j)
		{
			Vertex vertex;

			float c = cosf(j*dTheta);
			float s = sinf(j*dTheta);

			vertex.Position = XMFLOAT3(r*c, y, r*s);

			vertex.TexC.x = (float)j/sliceCount;
			vertex.TexC.y = 1.0f - (float)i/stackCount;

			// Cylinder can be parameterized as follows, where we introduce v
			// parameter that goes in the same direction as the v tex-coord
			// so that the bitangent goes in the same direction as the v tex-coord.
			//   Let r0 be the bottom radius and let r1 be the top radius.
			//   y(v) = h - hv for v in [0,1].
			//   r(v) = r1 + (r0-r1)v
			//
			//   x(t, v) = r(v)*cos(t)
			//   y(t, v) = h - hv
			//   z(t, v) = r(v)*sin(t)
			// 
			//  dx/dt = -r(v)*sin(t)
			//  dy/dt = 0
			//  dz/dt = +r(v)*cos(t)
			//
			//  dx/dv = (r0-r1)*cos(t)
			//  dy/dv = -h
			//  dz/dv = (r0-r1)*sin(t)

			// This is unit length.
			vertex.TangentU = XMFLOAT3(-s, 0.0f, c);

			float dr = bottomRadius-topRadius;
			XMFLOAT3 bitangent(dr*c, -height, dr*s);

			XMVECTOR T = XMLoadFloat3(&vertex.TangentU);
			XMVECTOR B = XMLoadFloat3(&bitangent);
			XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
			XMStoreFloat3(&vertex.Normal, N);

			meshData.Vertices.push_back(vertex);
		}
	}

	// Add one because we duplicate the first and last vertex per ring
	// since the texture coordinates are different.
	uint32 ringVertexCount = sliceCount+1;

	// Compute indices for each stack.
	for(uint32 i = 0; i < stackCount; ++i)
	{
		for(uint32 j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(i*ringVertexCount + j);
			meshData.Indices32.push_back((i+1)*ringVertexCount + j);
			meshData.Indices32.push_back((i+1)*ringVertexCount + j+1);

			meshData.Indices32.push_back(i*ringVertexCount + j);
			meshData.Indices32.push_back((i+1)*ringVertexCount + j+1);
			meshData.Indices32.push_back(i*ringVertexCount + j+1);
		}
	}

	BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
	BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

    return meshData;
}

void GeometryGenerator::BuildCylinderTopCap(float bottomRadius, float topRadius, float height,
											uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
	uint32 baseIndex = (uint32)meshData.Vertices.size();

	float y = 0.5f*height;
	float dTheta = 2.0f*XM_PI/sliceCount;

	// Duplicate cap ring vertices because the texture coordinates and normals differ.
	for(uint32 i = 0; i <= sliceCount; ++i)
	{
		float x = topRadius*cosf(i*dTheta);
		float z = topRadius*sinf(i*dTheta);

		// Scale down by the height to try and make top cap texture coord area
		// proportional to base.
		float u = x/height + 0.5f;
		float v = z/height + 0.5f;

		meshData.Vertices.push_back( Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v) );
	}

	// Cap center vertex.
	meshData.Vertices.push_back( Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f) );

	// Index of center vertex.
	uint32 centerIndex = (uint32)meshData.Vertices.size()-1;

	for(uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(centerIndex);
		meshData.Indices32.push_back(baseIndex + i+1);
		meshData.Indices32.push_back(baseIndex + i);
	}
}

void GeometryGenerator::BuildCylinderBottomCap(float bottomRadius, float topRadius, float height,
											   uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
	// 
	// Build bottom cap.
	//

	uint32 baseIndex = (uint32)meshData.Vertices.size();
	float y = -0.5f*height;

	// vertices of ring
	float dTheta = 2.0f*XM_PI/sliceCount;
	for(uint32 i = 0; i <= sliceCount; ++i)
	{
		float x = bottomRadius*cosf(i*dTheta);
		float z = bottomRadius*sinf(i*dTheta);

		// Scale down by the height to try and make top cap texture coord area
		// proportional to base.
		float u = x/height + 0.5f;
		float v = z/height + 0.5f;

		meshData.Vertices.push_back( Vertex(x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v) );
	}

	// Cap center vertex.
	meshData.Vertices.push_back( Vertex(0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f) );

	// Cache the index of center vertex.
	uint32 centerIndex = (uint32)meshData.Vertices.size()-1;

	for(uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(centerIndex);
		meshData.Indices32.push_back(baseIndex + i);
		meshData.Indices32.push_back(baseIndex + i+1);
	}
}

GeometryGenerator::MeshData GeometryGenerator::CreateGrid(float width, float depth, uint32 m, uint32 n)
{
    MeshData meshData;

	uint32 vertexCount = m*n;
	uint32 faceCount   = (m-1)*(n-1)*2;

	//
	// Create the vertices.
	//

	float halfWidth = 0.5f*width;
	float halfDepth = 0.5f*depth;

	float dx = width / (n-1);
	float dz = depth / (m-1);

	float du = 1.0f / (n-1);
	float dv = 1.0f / (m-1);

	meshData.Vertices.resize(vertexCount);
	for(uint32 i = 0; i < m; ++i)
	{
		float z = halfDepth - i*dz;
		for(uint32 j = 0; j < n; ++j)
		{
			float x = -halfWidth + j*dx;

			meshData.Vertices[i*n+j].Position = XMFLOAT3(x, 0.0f, z);
			meshData.Vertices[i*n+j].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
			meshData.Vertices[i*n+j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			meshData.Vertices[i*n+j].TexC.x = j*du;
			meshData.Vertices[i*n+j].TexC.y = i*dv;
		}
	}
 
    //
	// Create the indices.
	//

	meshData.Indices32.resize(faceCount*3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32 k = 0;
	for(uint32 i = 0; i < m-1; ++i)
	{
		for(uint32 j = 0; j < n-1; ++j)
		{
			meshData.Indices32[k]   = i*n+j;
			meshData.Indices32[k+1] = i*n+j+1;
			meshData.Indices32[k+2] = (i+1)*n+j;

			meshData.Indices32[k+3] = (i+1)*n+j;
			meshData.Indices32[k+4] = i*n+j+1;
			meshData.Indices32[k+5] = (i+1)*n+j+1;

			k += 6; // next quad
		}
	}

    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h, float depth)
{
    MeshData meshData;

	meshData.Vertices.resize(4);
	meshData.Indices32.resize(6);

	// Position coordinates specified in NDC space.
	meshData.Vertices[0] = Vertex(
        x, y - h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f);

	meshData.Vertices[1] = Vertex(
		x, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f);

	meshData.Vertices[2] = Vertex(
		x+w, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	meshData.Vertices[3] = Vertex(
		x+w, y-h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	meshData.Indices32[0] = 0;
	meshData.Indices32[1] = 1;
	meshData.Indices32[2] = 2;

	meshData.Indices32[3] = 0;
	meshData.Indices32[4] = 2;
	meshData.Indices32[5] = 3;

    return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateCone(float radius, float height, uint32 sliceCount, uint32 stackCount)
{
	return CreateCylinder(radius, 0.0f, height, sliceCount, stackCount);
}

GeometryGenerator::MeshData GeometryGenerator::CreateWedge(float width, float height, float depth)
{
	MeshData meshData;

	float w2 = 0.5f * width;
	float h2 = 0.5f * height;
	float d2 = 0.5f * depth;

	// Wedge vertices - like a box but with diagonal face instead of one side
	Vertex v[6];

	// Front face (z = -d2) - full rectangle
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[3] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Back vertices (z = +d2) - wedge shape (top vertex at center x)
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[5] = Vertex(0.0f, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.5f, 0.0f);

	meshData.Vertices.assign(&v[0], &v[6]);

	// Indices for 5 faces (front, back, bottom, left, diagonal)
	uint32 i[18];

	// Front face (2 triangles)
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 2; i[4] = 1; i[5] = 3;

	// Back face (1 triangle)
	i[6] = 4; i[7] = 5; i[8] = 2;

	// Bottom face (2 triangles)
	i[9] = 0; i[10] = 2; i[11] = 4;

	// Left face (2 triangles)
	i[12] = 0; i[13] = 4; i[14] = 1;
	i[15] = 1; i[16] = 4; i[17] = 5;

	meshData.Indices32.assign(&i[0], &i[18]);

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateTorus(float outerRadius, float tubeRadius, uint32 sliceCount, uint32 stackCount)
{
	MeshData meshData;

	// Generate torus vertices
	for (uint32 i = 0; i <= stackCount; ++i)
	{
		float phi = i * 2.0f * XM_PI / stackCount;

		for (uint32 j = 0; j <= sliceCount; ++j)
		{
			float theta = j * 2.0f * XM_PI / sliceCount;

			Vertex vertex;

			// Torus parametric equations
			vertex.Position.x = (outerRadius + tubeRadius * cosf(theta)) * cosf(phi);
			vertex.Position.y = tubeRadius * sinf(theta);
			vertex.Position.z = (outerRadius + tubeRadius * cosf(theta)) * sinf(phi);

			// Calculate tangent (derivative with respect to phi)
			vertex.TangentU.x = -(outerRadius + tubeRadius * cosf(theta)) * sinf(phi);
			vertex.TangentU.y = 0.0f;
			vertex.TangentU.z = (outerRadius + tubeRadius * cosf(theta)) * cosf(phi);

			XMVECTOR T = XMLoadFloat3(&vertex.TangentU);
			XMStoreFloat3(&vertex.TangentU, XMVector3Normalize(T));

			// Calculate normal
			XMFLOAT3 centerPoint(outerRadius * cosf(phi), 0.0f, outerRadius * sinf(phi));
			XMVECTOR C = XMLoadFloat3(&centerPoint);
			XMVECTOR P = XMLoadFloat3(&vertex.Position);
			XMVECTOR N = XMVector3Normalize(P - C);
			XMStoreFloat3(&vertex.Normal, N);

			// Texture coordinates
			vertex.TexC.x = (float)j / sliceCount;
			vertex.TexC.y = (float)i / stackCount;

			meshData.Vertices.push_back(vertex);
		}
	}

	// Generate indices
	uint32 ringVertexCount = sliceCount + 1;
	for (uint32 i = 0; i < stackCount; ++i)
	{
		for (uint32 j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(i * ringVertexCount + j);
			meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
			meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);

			meshData.Indices32.push_back(i * ringVertexCount + j);
			meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
			meshData.Indices32.push_back(i * ringVertexCount + j + 1);
		}
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreatePyramid(float baseWidth, float baseDepth, float height)
{
	MeshData meshData;

	float w2 = 0.5f * baseWidth;
	float d2 = 0.5f * baseDepth;
	float h2 = 0.5f * height;

	// Pyramid vertices
	Vertex v[5];

	// Base corners
	v[0] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[2] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);

	// Apex
	v[4] = Vertex(0.0f, +h2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f);

	meshData.Vertices.assign(&v[0], &v[5]);

	// Indices for 5 faces 
	uint32 i[18];

	// Base
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Front face
	i[6] = 0; i[7] = 4; i[8] = 1;

	// Right face
	i[9] = 1; i[10] = 4; i[11] = 2;

	// Back face
	i[12] = 2; i[13] = 4; i[14] = 3;

	// Left face
	i[15] = 3; i[16] = 4; i[17] = 0;

	meshData.Indices32.assign(&i[0], &i[18]);

	// Recalculate normals for side faces
	for (size_t idx = 6; idx < 18; idx += 3)
	{
		uint32 i0 = meshData.Indices32[idx];
		uint32 i1 = meshData.Indices32[idx + 1];
		uint32 i2 = meshData.Indices32[idx + 2];

		XMVECTOR p0 = XMLoadFloat3(&meshData.Vertices[i0].Position);
		XMVECTOR p1 = XMLoadFloat3(&meshData.Vertices[i1].Position);
		XMVECTOR p2 = XMLoadFloat3(&meshData.Vertices[i2].Position);

		XMVECTOR u = p1 - p0;
		XMVECTOR vVec = p2 - p0;
		XMVECTOR n = XMVector3Normalize(XMVector3Cross(u, vVec));

		// Update normals for all three vertices of this triangle
		XMStoreFloat3(&meshData.Vertices[i0].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i1].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i2].Normal, n);
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateDiamond(float height, float width)
{
	MeshData meshData;

	// Diamond (octahedron) - 6 vertices
	float halfHeight = 0.5f * height;
	float halfWidth = 0.5f * width;

	Vertex v[6];

	// Top point
	v[0] = Vertex(0.0f, +halfHeight, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);

	// Middle square
	v[1] = Vertex(+halfWidth, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f);
	v[2] = Vertex(0.0f, 0.0f, +halfWidth, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.5f, 1.0f);
	v[3] = Vertex(-halfWidth, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f);
	v[4] = Vertex(0.0f, 0.0f, -halfWidth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);

	// Bottom point
	v[5] = Vertex(0.0f, -halfHeight, 0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.5f, 1.0f);

	meshData.Vertices.assign(&v[0], &v[6]);

	// Indices for 8 triangular faces
	uint32 i[24];

	// Top 4 faces
	i[0] = 0; i[1] = 1; i[2] = 2;   // Top-front-right
	i[3] = 0; i[4] = 2; i[5] = 3;   // Top-back-right
	i[6] = 0; i[7] = 3; i[8] = 4;   // Top-back-left
	i[9] = 0; i[10] = 4; i[11] = 1; // Top-front-left

	// Bottom 4 faces
	i[12] = 5; i[13] = 2; i[14] = 1; // Bottom-front-right
	i[15] = 5; i[16] = 3; i[17] = 2; // Bottom-back-right
	i[18] = 5; i[19] = 4; i[20] = 3; // Bottom-back-left
	i[21] = 5; i[22] = 1; i[23] = 4; // Bottom-front-left

	meshData.Indices32.assign(&i[0], &i[24]);

	// Recalculate normals
	for (size_t idx = 0; idx < 24; idx += 3)
	{
		uint32 i0 = meshData.Indices32[idx];
		uint32 i1 = meshData.Indices32[idx + 1];
		uint32 i2 = meshData.Indices32[idx + 2];

		XMVECTOR p0 = XMLoadFloat3(&meshData.Vertices[i0].Position);
		XMVECTOR p1 = XMLoadFloat3(&meshData.Vertices[i1].Position);
		XMVECTOR p2 = XMLoadFloat3(&meshData.Vertices[i2].Position);

		XMVECTOR u = p1 - p0;
		XMVECTOR vVec = p2 - p0;
		XMVECTOR n = XMVector3Normalize(XMVector3Cross(u, vVec));

		XMStoreFloat3(&meshData.Vertices[i0].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i1].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i2].Normal, n);
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateTriangularPrism(float baseWidth, float baseDepth, float height)
{
	MeshData meshData;

	float w2 = 0.5f * baseWidth;
	float d2 = 0.5f * baseDepth;
	float h2 = 0.5f * height;

	// Triangular prism vertices
	Vertex v[6];

	// Front triangle
	v[0] = Vertex(0.0f, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);    // Top front
	v[1] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);    // Bottom left front
	v[2] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);    // Bottom right front

	// Back triangle 
	v[3] = Vertex(0.0f, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.5f, 0.0f);    // Top back
	v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);    // Bottom left back
	v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);    // Bottom right back

	meshData.Vertices.assign(&v[0], &v[6]);

	// Indices for 5 faces 
	uint32 i[24];

	// Front triangle
	i[0] = 0; i[1] = 1; i[2] = 2;

	// Back triangle
	i[3] = 3; i[4] = 5; i[5] = 4;

	// Bottom rectangle 
	i[6] = 1; i[7] = 4; i[8] = 5;
	i[9] = 1; i[10] = 5; i[11] = 2;

	// Left rectangle 
	i[12] = 1; i[13] = 0; i[14] = 4;
	i[15] = 0; i[16] = 3; i[17] = 4;

	// Right rectangle 
	i[18] = 2; i[19] = 5; i[20] = 0;
	i[21] = 0; i[22] = 5; i[23] = 3;

	meshData.Indices32.assign(&i[0], &i[24]);

	// Recalculate normals for side faces
	for (size_t idx = 6; idx < 24; idx += 3)
	{
		uint32 i0 = meshData.Indices32[idx];
		uint32 i1 = meshData.Indices32[idx + 1];
		uint32 i2 = meshData.Indices32[idx + 2];

		XMVECTOR p0 = XMLoadFloat3(&meshData.Vertices[i0].Position);
		XMVECTOR p1 = XMLoadFloat3(&meshData.Vertices[i1].Position);
		XMVECTOR p2 = XMLoadFloat3(&meshData.Vertices[i2].Position);

		XMVECTOR u = p1 - p0;
		XMVECTOR vVec = p2 - p0;
		XMVECTOR n = XMVector3Normalize(XMVector3Cross(u, vVec));

		XMStoreFloat3(&meshData.Vertices[i0].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i1].Normal, n);
		XMStoreFloat3(&meshData.Vertices[i2].Normal, n);
	}

	return meshData;
}

GeometryGenerator::MeshData GeometryGenerator::CreateHexagonalPrism(float radius, float height)
{
	MeshData meshData;

	float halfHeight = 0.5f * height;

	// Create hexagon vertices for top and bottom
	std::vector<Vertex> topVertices;
	std::vector<Vertex> bottomVertices;

	// Generate hexagon points 
	for (uint32 i = 0; i < 6; ++i)
	{
		float angle = i * 2.0f * XM_PI / 6.0f;
		float x = radius * cosf(angle);
		float z = radius * sinf(angle);

		// Top hexagon vertex
		Vertex topVertex;
		topVertex.Position = XMFLOAT3(x, halfHeight, z);
		topVertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		topVertex.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
		topVertex.TexC = XMFLOAT2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
		topVertices.push_back(topVertex);

		// Bottom hexagon vertex 
		Vertex bottomVertex;
		bottomVertex.Position = XMFLOAT3(x, -halfHeight, z);
		bottomVertex.Normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
		bottomVertex.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
		bottomVertex.TexC = XMFLOAT2(0.5f + 0.5f * cosf(angle), 0.5f + 0.5f * sinf(angle));
		bottomVertices.push_back(bottomVertex);
	}

	// Add center vertices for top and bottom caps
	Vertex topCenter;
	topCenter.Position = XMFLOAT3(0.0f, halfHeight, 0.0f);
	topCenter.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	topCenter.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
	topCenter.TexC = XMFLOAT2(0.5f, 0.5f);

	Vertex bottomCenter;
	bottomCenter.Position = XMFLOAT3(0.0f, -halfHeight, 0.0f);
	bottomCenter.Normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
	bottomCenter.TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);
	bottomCenter.TexC = XMFLOAT2(0.5f, 0.5f);

	// Store all vertices
	// Top vertices first, then top center, then bottom vertices, then bottom center
	for (const auto& v : topVertices)
		meshData.Vertices.push_back(v);

	meshData.Vertices.push_back(topCenter);

	for (const auto& v : bottomVertices)
		meshData.Vertices.push_back(v);

	meshData.Vertices.push_back(bottomCenter);

	// Create indices

	// 1. Top hexagon cap
	for (uint32 i = 0; i < 6; ++i)
	{
		meshData.Indices32.push_back(i);				// Current vertex
		meshData.Indices32.push_back(6);				// Top center
		meshData.Indices32.push_back((i + 1) % 6);		// Next vertex
	}

	// 2. Bottom hexagon cap
	uint32 bottomStart = 7;
	for (uint32 i = 0; i < 6; ++i)
	{
		meshData.Indices32.push_back(bottomStart + i);                // Current vertex
		meshData.Indices32.push_back(bottomStart + ((i + 1) % 6));    // Next vertex
		meshData.Indices32.push_back(13);                             // Bottom center
	}

	// 3. Six rectangular side faces
	for (uint32 i = 0; i < 6; ++i)
	{
		uint32 nextI = (i + 1) % 6;

		// First triangle of the rectangle - CCW winding
		meshData.Indices32.push_back(i);                          // Top current
		meshData.Indices32.push_back(nextI);                      // Top next
		meshData.Indices32.push_back(bottomStart + i);            // Bottom current

		// Second triangle of the rectangle - CCW winding
		meshData.Indices32.push_back(nextI);                      // Top next
		meshData.Indices32.push_back(bottomStart + nextI);        // Bottom next
		meshData.Indices32.push_back(bottomStart + i);            // Bottom current
	}

	// Recalculate normals for side faces
	for (uint32 i = 0; i < 6; ++i)
	{
		uint32 nextI = (i + 1) % 6;

		// Get the four corners of this rectangular side face
		uint32 topCurrent = i;
		uint32 topNext = nextI;
		uint32 bottomCurrent = bottomStart + i;
		uint32 bottomNext = bottomStart + nextI;

		// Calculate outward normal (pointing away from center)
		XMVECTOR pTop = XMLoadFloat3(&meshData.Vertices[topCurrent].Position);
		XMVECTOR pBottom = XMLoadFloat3(&meshData.Vertices[bottomCurrent].Position);
		XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

		// Vector from center to a point on this face
		XMVECTOR toFace = pTop - center;
		toFace = XMVector3Normalize(toFace);

		// The normal should point in the same direction as toFace (outward)
		XMFLOAT3 normal;
		XMStoreFloat3(&normal, toFace);

		// Apply the outward normal to all 4 vertices of this rectangle
		meshData.Vertices[topCurrent].Normal = normal;
		meshData.Vertices[topNext].Normal = normal;
		meshData.Vertices[bottomCurrent].Normal = normal;
		meshData.Vertices[bottomNext].Normal = normal;
	}

	return meshData;
}
