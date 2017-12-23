//
// 2017-12-23, jjuiddong
// QuadTree for Terrain
// (no multithread safe!!)
//
#pragma once

#include "quadtree.h"
#include "quadtile.h"


class cTerrainQuadTree
{
public:
	struct eDirection { enum Enum { NORTH, EAST, SOUTH, WEST }; };

	struct sQuadData
	{
		int level[4]; // Adjacent Quad Level (eDirection index)
		cQuadTile *tile;

		sQuadData::sQuadData() {
			ZeroMemory(level, sizeof(level));
			tile = NULL;
		}
	};


public:
	cTerrainQuadTree();
	cTerrainQuadTree(sRectf &rect);
	virtual ~cTerrainQuadTree();

	bool Create(graphic::cRenderer &renderer);

	void Render(graphic::cRenderer &renderer, const graphic::cFrustum &frustum
		, const int limitLevel, const int level
		, const Ray &ray
		, const Ray &mouseRay);


protected:
	void CalcSeamlessLevel();
	void CalcQuadEdgeLevel(sQuadTreeNode<sQuadData> *from, sQuadTreeNode<sQuadData> *to
		, const eDirection::Enum type);
	eDirection::Enum GetOpposite(const eDirection::Enum type);
	void BuildQuadTree(const graphic::cFrustum &frustum, const Ray &ray);
	void RenderTessellation(graphic::cRenderer &renderer, const graphic::cFrustum &frustum);
	void RenderQuad(graphic::cRenderer &renderer, const graphic::cFrustum &frustum, const Ray &ray);
	void RenderRect3D(graphic::cRenderer &renderer, const sRectf &rect, const graphic::cColor &color);
	bool ReadHeightMap(graphic::cRenderer &renderer, const char *fileName, const int bytesSize);
	inline int GetLevel(const float distance);
	inline bool IsContain(const graphic::cFrustum &frustum, const sRectf &rect);


public:
	cQuadTree<sQuadData> m_qtree;
	cQuadTileManager m_tileMgr;
	graphic::cMaterial m_mtrl;
	graphic::cVertexBuffer m_vtxBuff;
	graphic::cTexture m_heightTex;
	graphic::cShader11 m_shader;
	graphic::cText m_text;

	bool m_isShowTexture;
	bool m_isShowQuadTree;
	bool m_isShowLevel;
	bool m_isLight;
	int m_showQuadCount;
};
