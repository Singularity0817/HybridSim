
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "Flash_Block_Manager.h"
#include "Stats.h"
#include <assert.h>

namespace SSD_Components
{
	Flash_Block_Manager::Flash_Block_Manager(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: Flash_Block_Manager_Base(gc_and_wl_unit, max_allowed_block_erase_count, total_concurrent_streams_no, channel_count, chip_no_per_channel, die_no_per_chip,
			plane_no_per_die, block_no_per_plane, page_no_per_block)
	{
	}

	Flash_Block_Manager::Flash_Block_Manager(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int slc_block_no_per_plane, unsigned int page_no_per_slc_block)
		: Flash_Block_Manager_Base(gc_and_wl_unit, max_allowed_block_erase_count, total_concurrent_streams_no, channel_count, chip_no_per_channel, die_no_per_chip,
			plane_no_per_die, block_no_per_plane, page_no_per_block, slc_block_no_per_plane, page_no_per_slc_block)
	{
	}

	Flash_Block_Manager::~Flash_Block_Manager()
	{
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		std::cout << "Wrong Allocating Method..." << std::endl;
		std::cin.get();
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;		
		page_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued(page_address);
		if(plane_record->Data_wf[stream_id]->Current_page_write_index == pages_no_per_block)//The current write frontier block is written to the end
		{
			plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
			gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		}

		plane_record->Check_bookkeeping_correctness(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address, Flash_Technology_Type flash_type)
	{
		if(flash_type == Flash_Technology_Type::SLC)
		{
			if (Allocate_slc_block_and_page_in_plane_for_user_write(stream_id, page_address) == false)
			{
				PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
				std::cout << "allocating slc page failed, the free slc pages number is: " << plane_record->Free_slc_pages_count << std::endl;
				Allocate_tlc_block_and_page_in_plane_for_user_write(stream_id, page_address);
			}
		}
		else if(flash_type ==  Flash_Technology_Type::TLC)
		{
			Allocate_tlc_block_and_page_in_plane_for_user_write(stream_id, page_address);
		}
		else
		{
			std::cout << "Allocate_block_and_page_in_plane_for_user_write function WRONG..." << std::endl;
		}
	}

	bool Flash_Block_Manager::Allocate_slc_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		if (plane_record->Data_slc_wf[stream_id] == NULL)
		{
			plane_record->Data_slc_wf[stream_id] = plane_record->Get_a_free_slc_block(stream_id, false);
			if (plane_record->Data_slc_wf[stream_id] == NULL)
			{
				std::cout << "allocating slc page failed, the free slc pages number is: " << plane_record->Free_slc_pages_count << std::endl;
				return false;
			}
		}
		if (plane_record->Data_slc_wf[stream_id]->Current_page_write_index >= page_no_per_slc_block)
		{
			PRINT_ERROR("!!!Something wrong has happened! And the page index is collapsed !!!")
		}
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;
		//*ZWH*
		plane_record->Valid_slc_pages_count++;
		plane_record->Free_slc_pages_count--;
		//*ZWH*
		page_address.BlockID = plane_record->Data_slc_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->Data_slc_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued(page_address);
		if(plane_record->Data_slc_wf[stream_id]->Current_page_write_index == page_no_per_slc_block)//The current write frontier block is written to the end
		{
			plane_record->Data_slc_wf[stream_id] = plane_record->Get_a_free_slc_block(stream_id, false);
			gc_and_wl_unit->Check_data_migration_required(plane_record->Get_free_slc_block_pool_size(), page_address);
		}

		plane_record->Check_bookkeeping_correctness(page_address);
		Stats::UserSLCWriteCount++;
		Stats::UserSLCWriteCount_per_stream[stream_id]++;
		return true;
	}

	void Flash_Block_Manager::Allocate_tlc_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;
		//*ZWH*
		plane_record->Valid_tlc_pages_count++;
		plane_record->Free_tlc_pages_count--;
		//*ZWH*
		page_address.BlockID = plane_record->Data_tlc_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->Data_tlc_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued(page_address);
		if(plane_record->Data_tlc_wf[stream_id]->Current_page_write_index == page_no_per_tlc_block)//The current write frontier block is written to the end
		{
			plane_record->Data_tlc_wf[stream_id] = plane_record->Get_a_free_tlc_block(stream_id, false);
			gc_and_wl_unit->Check_tlc_gc_required(plane_record->Get_free_tlc_block_pool_size(), page_address);
		}

		plane_record->Check_bookkeeping_correctness(page_address);
		Stats::UserTLCWriteCount++;
		Stats::UserTLCWriteCount_per_stream[stream_id]++;
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;	
		//*ZWH*
		plane_record->Valid_tlc_pages_count++;
		plane_record->Free_tlc_pages_count--;
		//*ZWH*	
		page_address.BlockID = plane_record->GC_wf[stream_id]->BlockID;
		page_address.PageID = plane_record->GC_wf[stream_id]->Current_page_write_index++;
		program_transaction_issued(page_address);
		if (plane_record->GC_wf[stream_id]->Current_page_write_index == page_no_per_tlc_block)//The current write frontier block is written to the end
		{
			plane_record->GC_wf[stream_id] = plane_record->Get_a_free_tlc_block(stream_id, false);//Assign a new write frontier block
		}
		
		plane_record->Check_bookkeeping_correctness(page_address);
	}
	
	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses)
	{
		if(page_addresses.size() > page_no_per_tlc_block)
			PRINT_ERROR("Error while precondition a physical block: the size of the address list is larger than the pages_no_per_block!")
			
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];
		if (plane_record->Data_tlc_wf[stream_id]->Current_page_write_index > 0)
			PRINT_ERROR("Illegal operation: the Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning function should be executed for an erased block!")

		//Assign physical addresses
		for (int i = 0; i < page_addresses.size(); i++)
		{
			plane_record->Valid_pages_count++;
			plane_record->Free_pages_count--;
			page_addresses[i].BlockID = plane_record->Data_tlc_wf[stream_id]->BlockID;
			page_addresses[i].PageID = plane_record->Data_tlc_wf[stream_id]->Current_page_write_index++;
			plane_record->Check_bookkeeping_correctness(page_addresses[i]);
		}

		//Invalidate the remaining pages in the block
		NVM::FlashMemory::Physical_Page_Address target_address(plane_address);
		while (plane_record->Data_tlc_wf[stream_id]->Current_page_write_index < page_no_per_tlc_block)
		{
			plane_record->Free_pages_count--;
			target_address.BlockID = plane_record->Data_tlc_wf[stream_id]->BlockID;
			target_address.PageID = plane_record->Data_tlc_wf[stream_id]->Current_page_write_index++;
			Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
			plane_record->Check_bookkeeping_correctness(plane_address);
		}

		//Update the write frontier
		plane_record->Data_tlc_wf[stream_id] = plane_record->Get_a_free_tlc_block(stream_id, false);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;
		//*ZWH*
		plane_record->Valid_tlc_pages_count++;
		plane_record->Free_tlc_pages_count--;
		//*ZWH*
		page_address.BlockID = plane_record->Translation_wf[streamID]->BlockID;
		page_address.PageID = plane_record->Translation_wf[streamID]->Current_page_write_index++;
		//program_transaction_issued(page_address);
		if (plane_record->Translation_wf[streamID]->Current_page_write_index == page_no_per_tlc_block)//The current write frontier block for translation pages is written to the end
		{
			plane_record->Translation_wf[streamID] = plane_record->Get_a_free_tlc_block(streamID, true);//Assign a new write frontier block
			if (!is_for_gc)
			{
				//gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
				gc_and_wl_unit->Check_tlc_gc_required(plane_record->Get_free_tlc_block_pool_size(), page_address);
			}
		}
		plane_record->Check_bookkeeping_correctness(page_address);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id)
		{
			std::cout << std::endl << " WARNING: invalidating page with wrong Stream_id " << page_address.ChannelID << ":" << page_address.ChipID << ":" << page_address.DieID << ":" << page_address.PlaneID << ":" << page_address.BlockID << ":" << page_address.PageID << std::endl;
			std::cin.get();
			return;
		}
		if (Is_page_valid(&(plane_record->Blocks[page_address.BlockID]), page_address.PageID) == false)
			return;
		plane_record->Invalid_pages_count++;
		plane_record->Valid_pages_count--;
		if (plane_record->Blocks[page_address.BlockID].Flash_type == Flash_Technology_Type::SLC)
		{
			plane_record->Invalid_slc_pages_count++;
			plane_record->Valid_slc_pages_count--;
		}
		else
		{
			plane_record->Invalid_tlc_pages_count++;
			plane_record->Valid_tlc_pages_count--;
		}
		
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id)
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << plane_record->Blocks[page_address.BlockID].Stream_id << ":" <<stream_id)
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id)
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block_for_preconditioning function! The accessed block is not allocated to stream " << stream_id)
			plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	void Flash_Block_Manager::Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		Block_Pool_Slot_Type* block = &(plane_record->Blocks[block_address.BlockID]);
		plane_record->Free_pages_count += block->Invalid_page_count;
		plane_record->Invalid_pages_count -= block->Invalid_page_count;
		if (block->Flash_type == Flash_Technology_Type::SLC)
		{
			plane_record->Free_slc_pages_count += block->Pages_count;
			plane_record->Invalid_slc_pages_count -= block->Invalid_page_count;
		}
		else if (block->Flash_type == Flash_Technology_Type::TLC)
		{
			plane_record->Free_tlc_pages_count += block->Pages_count;
			plane_record->Invalid_tlc_pages_count -= block->Invalid_page_count;
		}
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]--;
		block->Erase();
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]++;
		if (block->Flash_type == Flash_Technology_Type::SLC)
		{
			plane_record->Add_to_free_slc_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		}
		else if (block->Flash_type == Flash_Technology_Type::TLC)
		{
			plane_record->Add_to_free_tlc_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		}
		//plane_record->Add_to_free_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		plane_record->Check_bookkeeping_correctness(block_address);
	}

	inline unsigned int Flash_Block_Manager::Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		//return (unsigned int) plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_block_pool.size();
		return (unsigned int) (plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_slc_block_pool.size() +
			plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_tlc_block_pool.size());
	}

	inline unsigned int Flash_Block_Manager::Get_slc_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		//return (unsigned int) plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_block_pool.size();
		return (unsigned int) (plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_slc_block_pool.size());
	}

	inline unsigned int Flash_Block_Manager::Get_tlc_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		//return (unsigned int) plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_block_pool.size();
		return (unsigned int) (plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_tlc_block_pool.size());
	}
}