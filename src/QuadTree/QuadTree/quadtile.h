//
// 2017-12-23, jjuiddong
// QuadTree, Tile, MemoryPool Manager
//
#pragma once


//--------------------------------------------------------------------
// cQuadTile
class cQuadTile
{
public:
	cQuadTile();
	virtual ~cQuadTile();

	bool Create(graphic::cRenderer &renderer, const int level, const int xLoc, const int yLoc
		, const sRectf &rect
		, const char *textureFileName);

	void Render(graphic::cRenderer &renderer
		, const int adjLevel[4]
		, const XMMATRIX &tm = graphic::XMIdentity);


public:
	int m_level;
	Vector2i m_loc; // x,y Location in Quad Grid Coordinate in specific level
	sRectf m_rect; // 3d space rect
	int m_replaceTexLv; // replace texture level (upper level), default: -1
	float m_uvs[4]; // left-top uv, right-bottom uv coordinate
	graphic::cTexture *m_texture; // reference
	graphic::cTexture *m_replaceTex; // reference
	graphic::cVertexBuffer *m_vtxBuff; // reference
};



//--------------------------------------------------------------------
// cQuadTileManager
class cTerrainQuadTree;
class cQuadTileManager
{
public:
	cQuadTileManager();
	virtual ~cQuadTileManager();

	bool Update(const float deltaSeconds);
	
	cQuadTile* GetTile(graphic::cRenderer &renderer
		, cTerrainQuadTree &terrain
		, const int level, const int xLoc, const int yLoc
		, const sRectf &rect );

	void Clear();


protected:
	bool ReplaceParentTexture(cTerrainQuadTree &terrain
		, cQuadTile *tile
		, const int level, const int xLoc, const int yLoc
		, const sRectf &rect);


public:
	map<u_int, cQuadTile*> m_tiles;
	graphic::cVertexBuffer m_vtxBuff;
};
