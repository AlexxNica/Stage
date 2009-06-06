#pragma once
/*
  region.hh
  data structures supporting multi-resolution ray tracing in world class.
  Copyright Richard Vaughan 2008
*/

#include "stage.hh"

#include <algorithm>
#include <vector>

namespace Stg 
{

// a bit of experimenting suggests that these values are fast. YMMV.
 	const int32_t RBITS( 4 ); // regions contain (2^RBITS)^2 pixels
 	const int32_t SBITS( 5 );// superregions contain (2^SBITS)^2 regions
 	const int32_t SRBITS( RBITS+SBITS );
		
 	const int32_t REGIONWIDTH( 1<<RBITS );
 	const int32_t REGIONSIZE( REGIONWIDTH*REGIONWIDTH );

 	const int32_t SUPERREGIONWIDTH( 1<<SBITS );
 	const int32_t SUPERREGIONSIZE( SUPERREGIONWIDTH*SUPERREGIONWIDTH );
		
	/** (x & CELLMASK) converts a global cell index into a local cell
			index in a region */
	const int32_t CELLMASK( ~((~0x00)<< RBITS ));
	/** (x & REGIONMASK)converts a global cell index into a local cell
			index in a region */
	const int32_t REGIONMASK( ~((~0x00)<< SRBITS ));
		
	inline int32_t GETCELL( const int32_t x ) { return( x & CELLMASK); }
	inline int32_t GETREG(  const int32_t x ) { return( ( x & REGIONMASK ) >> RBITS); }
	inline int32_t GETSREG( const int32_t x ) { return( x >> SRBITS); }

class Cell 
{
  friend class Region;
  friend class SuperRegion;
  friend class World;
  friend class Block;
  
private:
  Region* region;  
  std::vector<Block*> blocks;
  bool boundary;

public:
  Cell() 
	 : region( NULL),
		blocks() 
  { 
  }  
  
  void RemoveBlock( Block* b );
  void AddBlock( Block* b );  
  void AddBlockNoRecord( Block* b );
};  

class Region
{
public:
  
  Cell cells[ REGIONSIZE ];
  SuperRegion* superregion;	
  unsigned long count; // number of blocks rendered into these cells
  
  Region();
  ~Region();
  
  Cell* GetCell( int32_t x, int32_t y ) const
  { 
		return( (Cell*)&cells[ x + (y*REGIONWIDTH) ] ); 
  }
       
	void DecrementOccupancy();
	void IncrementOccupancy();
};
  
	class SuperRegion
  {
		friend class World;
		friend class Model;
	 
  private:

	 Region regions[SUPERREGIONSIZE];
	 unsigned long count; // number of blocks rendered into these regions
	 
	 stg_point_int_t origin;
	 World* world;
	 
  public:

	 SuperRegion( World* world, stg_point_int_t origin );
	 ~SuperRegion();
		
		const Region* GetRegion( int32_t x, int32_t y ) const
		{
			return( &regions[ x + (y*SUPERREGIONWIDTH) ] );
		}
		
	 void Draw( bool drawall );
	 void Floor();
	 
	 void DecrementOccupancy(){ --count; };
	 void IncrementOccupancy(){ ++count; };
  };


inline void printvec( std::vector<Block*>& vec )
  {
	 printf( "Vec: ");
	 for( size_t i=0; i<vec.size(); i++ )
		printf( "%p ", vec[i] );
	 puts( "" );
  }

inline void Cell::RemoveBlock( Block* b )
{
  // linear time removal, but these vectors are very short, usually 1
  // or 2 elements.  Fast removal - our strategy is to copy the last
  // item in the vector over the item we want to remove, then pop off
  // the tail. This avoids moving the other items in the vector. Saves
  // maybe 1 or 2% run time in my tests.
  
  // find the value in the vector     
  //   printf( "\nremoving %p\n", b );
  //   puts( "before" );
  //   printvec( blocks );
  
  // copy the last item in the vector to overwrite this one
  copy_backward( blocks.end(), blocks.end(), std::find( blocks.begin(), blocks.end(), b ));
  blocks.pop_back(); // and remove the redundant copy at the end of
							// the vector
  
  region->DecrementOccupancy();
}

inline void Cell::AddBlock( Block* b )
{
  // constant time prepend
  blocks.push_back( b );
  region->IncrementOccupancy();
  b->RecordRendering( this );
}


}; // namespace Stg
