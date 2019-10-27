#include "Plane.h"

namespace NVM
{
	namespace FlashMemory
	{
		Plane::Plane(unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock) :
			Read_count(0), Progam_count(0), Erase_count(0)
		{
			Healthy_block_no = BlocksNoPerPlane;
			Blocks = new Block*[BlocksNoPerPlane];
			for (unsigned int i = 0; i < BlocksNoPerPlane; i++)
				Blocks[i] = new Block(PagesNoPerBlock, i);
			Allocated_streams = NULL;
		}

		Plane::Plane(unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock, unsigned int SlcBlocksNoPerPlane, unsigned int PagesNoPerSlcBlock) :
			Read_count(0), Progam_count(0), Erase_count(0)
		{
			Healthy_block_no = BlocksNoPerPlane;
			Blocks = new Block*[BlocksNoPerPlane];
			Slc_block_no = SlcBlocksNoPerPlane;
			Tlc_block_no = Healthy_block_no - SlcBlocksNoPerPlane;
			for (unsigned int i = 0; i < SlcBlocksNoPerPlane; i++)
				Blocks[i] = new Block(PagesNoPerSlcBlock, i, Flash_Technology_Type::SLC);
			for (unsigned int i = SlcBlocksNoPerPlane; i < BlocksNoPerPlane; i++)
				Blocks[i] = new Block(PagesNoPerBlock, i, Flash_Technology_Type::TLC);
			Allocated_streams = NULL;
		}

		Plane::~Plane()
		{
			for (unsigned int i = 0; i < Healthy_block_no; i++)
				delete Blocks[i];
			delete[] Blocks;
		}
	}
}