#ifndef PLANE_H
#define PLANE_H

#include "../NVM_Types.h"
#include "FlashTypes.h"
#include "Block.h"
#include "Flash_Command.h"

namespace NVM
{
	namespace FlashMemory
	{
		class Plane
		{
		public:
			Plane(unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock);
			Plane(unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock, unsigned int SlcBlocksNoPerPlane, unsigned int PagesNoPerSlcBlock);
			~Plane();
			Block** Blocks;
			unsigned int Healthy_block_no;
			unsigned int Slc_block_no;
			unsigned int Tlc_block_no;
			unsigned long Read_count;                     //how many read count in the process of workload
			unsigned long Progam_count;
			unsigned long Erase_count;
			unsigned long Slc_erase_count, Tlc_erase_count;
			stream_id_type* Allocated_streams;
			Flash_Technology_Type Get_Flash_Type_of_Block(flash_block_ID_type blockID)
			{
				return Blocks[blockID]->flash_type;
			}
		};
	}
}
#endif // !PLANE_H
