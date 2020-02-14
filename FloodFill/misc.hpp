#pragma once

struct CSize
{
	int cx, cy;
	inline static CSize zero()
	{
		return CSize{ 0, 0 };
	}
	inline bool operator==( const CSize that ) const
	{
		return cx == that.cx && cy == that.cy;
	}
};

struct CPoint
{
	int x, y;
	inline static CPoint zero()
	{
		return CPoint{ 0, 0 };
	}
};

inline bool isInBounds( CSize size, CPoint pt )
{
	return pt.x >= 0 && pt.y >= 0 && pt.x < size.cx && pt.y < size.cy;
}