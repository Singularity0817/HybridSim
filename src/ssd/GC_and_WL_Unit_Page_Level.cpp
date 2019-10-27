#include <math.h>
#include <vector>
#include <set>
#include "GC_and_WL_Unit_Page_Level.h"
#include "Flash_Block_Manager.h"
#include "FTL.h"
#include "Host_Interface_NVMe.h"

namespace SSD_Components
{

	GC_and_WL_Unit_Page_Level::GC_and_WL_Unit_Page_Level(const sim_object_id_type& id,
		Address_Mapping_Unit_Base* address_mapping_unit, Flash_Block_Manager_Base* block_manager, TSU_Base* tsu, NVM_PHY_ONFI* flash_controller, 
		GC_Block_Selection_Policy_Type block_selection_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int Page_no_per_block, unsigned int sectors_per_page, 
		bool use_copyback, double rho, unsigned int max_ongoing_gc_reqs_per_plane, bool dynamic_wearleveling_enabled, bool static_wearleveling_enabled, unsigned int static_wearleveling_threshold, int seed)
		: GC_and_WL_Unit_Base(id, address_mapping_unit, block_manager, tsu, flash_controller, block_selection_policy, gc_threshold, preemptible_gc_enabled, gc_hard_threshold,
		ChannelCount, chip_no_per_channel, die_no_per_chip, plane_no_per_die, block_no_per_plane, Page_no_per_block, sectors_per_page, use_copyback, rho, max_ongoing_gc_reqs_per_plane, 
			dynamic_wearleveling_enabled, static_wearleveling_enabled, static_wearleveling_threshold, seed)
	{
		rga_set_size = (unsigned int)log2(block_no_per_plane);
	}

	GC_and_WL_Unit_Page_Level::GC_and_WL_Unit_Page_Level(const sim_object_id_type& id,
		Address_Mapping_Unit_Base* address_mapping_unit, Flash_Block_Manager_Base* block_manager, TSU_Base* tsu, NVM_PHY_ONFI* flash_controller, 
		GC_Block_Selection_Policy_Type block_selection_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold, double slc_gc_threshold, double tlc_gc_threshold,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int Page_no_per_block, unsigned int sectors_per_page, unsigned int slc_block_no_per_plane, unsigned int page_no_per_slc_block,
		bool use_copyback, double rho, unsigned int max_ongoing_gc_reqs_per_plane, bool dynamic_wearleveling_enabled, bool static_wearleveling_enabled, unsigned int static_wearleveling_threshold, int seed)
		: GC_and_WL_Unit_Base(id, address_mapping_unit, block_manager, tsu, flash_controller, block_selection_policy, gc_threshold, preemptible_gc_enabled, gc_hard_threshold, slc_gc_threshold, tlc_gc_threshold,
		ChannelCount, chip_no_per_channel, die_no_per_chip, plane_no_per_die, block_no_per_plane, Page_no_per_block, sectors_per_page, slc_block_no_per_plane, page_no_per_slc_block, use_copyback, rho, max_ongoing_gc_reqs_per_plane, 
			dynamic_wearleveling_enabled, static_wearleveling_enabled, static_wearleveling_threshold, seed)
	{
		rga_set_size = (unsigned int)log2(block_no_per_plane);
		if (slc_block_no_per_plane == 0)
			slc_rga_set_size = 0;
		else
			slc_rga_set_size = (unsigned int)log2(slc_block_no_per_plane);
		tlc_rga_set_size = (unsigned int)log2(tlc_block_no_per_plane);
	}
	
	bool GC_and_WL_Unit_Page_Level::GC_is_in_urgent_mode(const NVM::FlashMemory::Flash_Chip* chip)
	{
		//*ZWH*
		//GC will not enter urgent mode now!!!!
		//*ZWH*
		return false;
		//*ZWH*
		
		if (!preemptible_gc_enabled)
			return true;

		NVM::FlashMemory::Physical_Page_Address addr;
		addr.ChannelID = chip->ChannelID; addr.ChipID = chip->ChipID;
		for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++)
			for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++)
			{
				addr.DieID = die_id; addr.PlaneID = plane_id;
				if (block_manager->Get_pool_size(addr) < block_pool_gc_hard_threshold)
					return true;
			}
		
		return false;
	}

	void GC_and_WL_Unit_Page_Level::Check_gc_required(const unsigned int free_block_pool_size, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (free_block_pool_size < block_pool_gc_threshold)
		{
			std::cout << "WRONG!!!! ENTERING Check_gc_required!!!!!" << std::endl;
			std::cin.get();
			flash_block_ID_type gc_candidate_block_id = block_manager->Get_coldest_block_id(plane_address);
			PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);

			if (pbke->Ongoing_erase_operations.size() >= max_ongoing_gc_reqs_per_plane)
				return;

			switch (block_selection_policy)
			{
			case SSD_Components::GC_Block_Selection_Policy_Type::GREEDY://Find the set of blocks with maximum number of invalid pages and no free pages
			{
				gc_candidate_block_id = 0;
				if (pbke->Ongoing_erase_operations.find(0) != pbke->Ongoing_erase_operations.end())
					gc_candidate_block_id++;
				for (flash_block_ID_type block_id = 1; block_id < block_no_per_plane; block_id++)
				{
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block
						&& is_safe_gc_wl_candidate(pbke, block_id))
						gc_candidate_block_id = block_id;
				}
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RGA:
			{
				std::set<flash_block_ID_type> random_set;
				while (random_set.size() < rga_set_size)
				{
					flash_block_ID_type block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					if (pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
						&& is_safe_gc_wl_candidate(pbke, block_id))
						random_set.insert(block_id);
				}
				gc_candidate_block_id = *random_set.begin();
				for(auto &block_id : random_set)
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block)
						gc_candidate_block_id = block_id;
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;
				while (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id) && repeat++ < block_no_per_plane)//A write frontier block should not be selected for garbage collection
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_P:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//A write frontier block or a block with free pages should not be selected for garbage collection
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block || !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_PP:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//The selected gc block should have a minimum number of invalid pages
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block 
					|| pbke->Blocks[gc_candidate_block_id].Invalid_page_count < random_pp_threshold
					|| !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::FIFO:
				gc_candidate_block_id = pbke->Block_usage_history.front();
				pbke->Block_usage_history.pop();
				break;
			default:
				break;
			}
			
			if (pbke->Ongoing_erase_operations.find(gc_candidate_block_id) != pbke->Ongoing_erase_operations.end())//This should never happen, but we check it here for safty
				return;
			
			NVM::FlashMemory::Physical_Page_Address gc_candidate_address(plane_address);
			gc_candidate_address.BlockID = gc_candidate_block_id;
			Block_Pool_Slot_Type* block = &pbke->Blocks[gc_candidate_block_id];
			if (block->Current_page_write_index == 0 || block->Invalid_page_count == 0)//No invalid page to erase
				return;
			
			//Run the state machine to protect against race condition
			block_manager->GC_WL_started(gc_candidate_address);
			pbke->Ongoing_erase_operations.insert(gc_candidate_block_id);
			address_mapping_unit->Set_barrier_for_accessing_physical_block(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing
			if (block_manager->Can_execute_gc_wl(gc_candidate_address))//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
			{
				Stats::Total_gc_executions++;
				tsu->Prepare_for_transaction_submit();

				NVM_Transaction_Flash_ER* gc_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[gc_candidate_block_id].Stream_id, gc_candidate_address);
				if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
				{
					NVM_Transaction_Flash_RD* gc_read = NULL;
					NVM_Transaction_Flash_WR* gc_write = NULL;
					for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
					{
						if (block_manager->Is_page_valid(block, pageID))
						{
							Stats::Total_page_movements_for_gc++;
							gc_candidate_address.PageID = pageID;
							if (use_copyback)
							{
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
								tsu->Submit_transaction(gc_write);
							}
							else
							{
								gc_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), gc_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, NO_PPA, gc_candidate_address, NULL, 0, gc_read, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
								gc_write->RelatedErase = gc_erase_tr;
								gc_read->RelatedWrite = gc_write;
								tsu->Submit_transaction(gc_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
							}
							gc_erase_tr->Page_movement_activities.push_back(gc_write);
						}
					}
				}
				block->Erase_transaction = gc_erase_tr;
				tsu->Submit_transaction(gc_erase_tr);
				tsu->Schedule();
			}
		}
	}

	void GC_and_WL_Unit_Page_Level::Check_data_migration_required(const unsigned int free_slc_block_pool_size, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		bool allow_data_migration = false;
		if (dm_policy == Data_Migration_Policy::QPC_DM)
		{
			unsigned int queued_requests_num, queued_transactions_num;
			queued_requests_num = ((SSD_Components::Host_Interface_NVMe *)(address_mapping_unit->ftl->host_interface))->Return_queued_requests_num();
			queued_transactions_num = ((SSD_Components::Host_Interface_NVMe *)(address_mapping_unit->ftl->host_interface))->Return_queued_transactions_num();
			int preserved_plane_num = total_plane_num - queued_transactions_num/3 - 5;
			if (preserved_plane_num < 0)
			{
				preserved_plane_num = 0;
			}
			if (plane_num_doing_data_migration < preserved_plane_num)
			{
				PlaneBookKeepingType* pbke_temp = block_manager->Get_plane_bookkeeping_entry(plane_address);
				if (pbke_temp->Doing_garbage_collection == true)
				{
					return;
				}
				else if (pbke_temp->Data_migration_should_be_terminated == true)
				{
					return;
				}
				else if (pbke_temp->Doing_data_migration == true
					&& pbke_temp->Triggered_data_migration < block_manager->Max_data_migration_trigger_one_time
					&& Simulator->Time() - Simulator->Last_request_triggerred_time >= dm_interval
					&& free_slc_block_pool_size < (unsigned int)(slc_block_no_per_plane * 0.8))
				{
					allow_data_migration = true;
				}
				else if (pbke_temp->Doing_data_migration == false
					&& Simulator->Time() - Simulator->Last_request_triggerred_time >= dm_interval
					&& free_slc_block_pool_size < (unsigned int)(slc_block_no_per_plane * 0.8))
				{
					allow_data_migration = true;
				}
				else if (pbke_temp->Doing_data_migration == true && pbke_temp->Triggered_data_migration < block_manager->Max_data_migration_trigger_one_time && free_slc_block_pool_size < slc_block_pool_gc_threshold)
				{
					allow_data_migration = true;
				}
				else if (pbke_temp->Doing_data_migration == false && free_slc_block_pool_size == 0)
				{
					allow_data_migration = true;
				}
				else if (pbke_temp->Doing_data_migration == true)
				{
					if (pbke_temp->Triggered_data_migration == pbke_temp->Completed_data_migration)
					{
						pbke_temp->Doing_data_migration = false;
						pbke_temp->Triggered_data_migration = 0;
						pbke_temp->Completed_data_migration = 0;
						pbke_temp->Data_migration_should_be_terminated = false;
						plane_num_doing_data_migration--;
					}
					else
					{
						pbke_temp->Data_migration_should_be_terminated = true;
					}
				}
			}
			else if (plane_num_doing_data_migration > preserved_plane_num)
			{
				PlaneBookKeepingType* pbke_temp = block_manager->Get_plane_bookkeeping_entry(plane_address);
				if (pbke_temp->Doing_data_migration == true)
				{
					if (pbke_temp->Triggered_data_migration == pbke_temp->Completed_data_migration)
					{
						pbke_temp->Doing_data_migration = false;
						pbke_temp->Triggered_data_migration = 0;
						pbke_temp->Completed_data_migration = 0;
						pbke_temp->Data_migration_should_be_terminated = false;
						plane_num_doing_data_migration--;
					}
					else
					{
						pbke_temp->Data_migration_should_be_terminated = true;
					}
				}
			}
			else if (plane_num_doing_data_migration == preserved_plane_num)
			{
				PlaneBookKeepingType* pbke_temp = block_manager->Get_plane_bookkeeping_entry(plane_address);
				if (pbke_temp->Doing_garbage_collection == true)
				{
					return;
				}
				else if (pbke_temp->Data_migration_should_be_terminated == true)
				{
					return;
				}
				else if (pbke_temp->Doing_data_migration == true)
				{
					if (pbke_temp->Triggered_data_migration < block_manager->Max_data_migration_trigger_one_time)
					{
						allow_data_migration = true;
					}
					else if (pbke_temp->Triggered_data_migration == pbke_temp->Completed_data_migration)
					{
						pbke_temp->Doing_data_migration = false;
						pbke_temp->Triggered_data_migration = 0;
						pbke_temp->Completed_data_migration = 0;
						pbke_temp->Data_migration_should_be_terminated = false;
						plane_num_doing_data_migration--;
					}
					else
					{
						pbke_temp->Data_migration_should_be_terminated = true;
					}
				}
			}
		}
		else if (dm_policy == Data_Migration_Policy::IDLE)
		{
			if (Simulator->Time() - Simulator->Last_request_triggerred_time >= dm_interval && free_slc_block_pool_size < (unsigned int)(slc_block_no_per_plane * 0.8))
				allow_data_migration = true;
		}
		else if (dm_policy == Data_Migration_Policy::ALWAYS)
		{
			if (free_slc_block_pool_size < slc_block_pool_gc_threshold)
				allow_data_migration = true;
		}
		else
		{
			std::cout << "Wrong Data Migration Policy..." << std::endl;
		}
		

		if (allow_data_migration == true)
		{
			flash_block_ID_type gc_candidate_block_id = block_manager->Get_coldest_slc_block_id(plane_address);
			PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);

			if (pbke->Ongoing_slc_erase_operations.size() >= max_ongoing_gc_reqs_per_plane)
				return;
			bool flag = false;
			switch (block_selection_policy)
			{
			case SSD_Components::GC_Block_Selection_Policy_Type::GREEDY://Find the set of blocks with maximum number of invalid pages and no free pages
			{
				gc_candidate_block_id = 0;
				for (flash_block_ID_type block_id = 0; block_id < slc_block_no_per_plane; block_id++)
				{
					if (pbke->Ongoing_slc_erase_operations.find(block_id) != pbke->Ongoing_slc_erase_operations.end())
						continue;
					else if (flag == false)
					{
						if (pbke->Blocks[block_id].Current_page_write_index == pages_no_per_slc_block 
							&& is_safe_gc_wl_candidate(pbke, block_id) == true)
						{
							gc_candidate_block_id = block_id;
							flag = true;
						}
					}
					else
					{
						if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_slc_block
						&& is_safe_gc_wl_candidate(pbke, block_id) == true)
						{
							gc_candidate_block_id = block_id;
						}
					}
				}
				if (flag == false)
					return;
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RGA:
			{
				std::set<flash_block_ID_type> random_set;
				while (random_set.size() < slc_rga_set_size)
				{
					flash_block_ID_type block_id = random_generator.Uniform_uint(0, slc_block_no_per_plane - 1);
					if (pbke->Ongoing_slc_erase_operations.find(block_id) == pbke->Ongoing_slc_erase_operations.end()
						&& is_safe_gc_wl_candidate(pbke, block_id))
						random_set.insert(block_id);
				}
				gc_candidate_block_id = *random_set.begin();
				for(auto &block_id : random_set)
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_slc_block)
						gc_candidate_block_id = block_id;
				break;
			}
			/*
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;
				while (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id) && repeat++ < block_no_per_plane)//A write frontier block should not be selected for garbage collection
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_P:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//A write frontier block or a block with free pages should not be selected for garbage collection
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block || !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_PP:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//The selected gc block should have a minimum number of invalid pages
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block 
					|| pbke->Blocks[gc_candidate_block_id].Invalid_page_count < random_pp_threshold
					|| !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::FIFO:
				gc_candidate_block_id = pbke->Block_usage_history.front();
				pbke->Block_usage_history.pop();
				break;
			*/
			default:
			{
				std::cout << "Wrong GC Policy..." << std::endl;
				break;
			}
			}
			
			if (pbke->Ongoing_slc_erase_operations.find(gc_candidate_block_id) != pbke->Ongoing_slc_erase_operations.end())//This should never happen, but we check it here for safty
				return;
			
			NVM::FlashMemory::Physical_Page_Address gc_candidate_address(plane_address);
			gc_candidate_address.BlockID = gc_candidate_block_id;
			Block_Pool_Slot_Type* block = &pbke->Blocks[gc_candidate_block_id];
			if (block->Current_page_write_index == 0 || block->Invalid_page_count == 0)//No invalid page to erase
				return;
			
			//Run the state machine to protect against race condition
			block_manager->GC_WL_started(gc_candidate_address);
			pbke->Ongoing_slc_erase_operations.insert(gc_candidate_block_id);
			address_mapping_unit->Set_barrier_for_accessing_physical_block(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing

			if (pbke->Doing_data_migration == false)
			{
				plane_num_doing_data_migration++;
				pbke->Doing_data_migration = true;
			}
			pbke->Triggered_data_migration++;

			if (block_manager->Can_execute_gc_wl(gc_candidate_address))//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
			{
				Stats::Total_gc_executions++;
				Stats::Data_migration_executions++;
				tsu->Prepare_for_transaction_submit();

				NVM_Transaction_Flash_ER* gc_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[gc_candidate_block_id].Stream_id, gc_candidate_address);
				unsigned int valid_page_count = 0;
				if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
				{
					NVM_Transaction_Flash_RD* gc_read = NULL;
					NVM_Transaction_Flash_WR* gc_write = NULL;
					for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
					{
						if (block_manager->Is_page_valid(block, pageID))
						{
							valid_page_count++;
							Stats::Total_page_movements_for_gc++;
							Stats::Data_migration_page_movements++;
							gc_candidate_address.PageID = pageID;
							if (use_copyback)
							{
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
								tsu->Submit_transaction(gc_write);
							}
							else
							{
								gc_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), gc_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, NO_PPA, gc_candidate_address, NULL, 0, gc_read, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
								gc_write->RelatedErase = gc_erase_tr;
								gc_read->RelatedWrite = gc_write;
								tsu->Submit_transaction(gc_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
							}
							gc_erase_tr->Page_movement_activities.push_back(gc_write);
						}
					}
				}
				block->Erase_transaction = gc_erase_tr;
				tsu->Submit_transaction(gc_erase_tr);
				tsu->Schedule();
			}
		}
	}

	void GC_and_WL_Unit_Page_Level::Try_check_data_migration_required()
	{
		unsigned int rand_plane = rand() % plane_no_per_die;
		unsigned int rand_die = rand() % die_no_per_chip;
		unsigned int rand_chip = rand() % chip_no_per_channel;
		unsigned int rand_chan = rand() % channel_count;
		for (flash_plane_ID_type plane_id = 0; plane_id < plane_no_per_die; plane_id++)
		{
			for (flash_die_ID_type die_id = 0; die_id < die_no_per_chip; die_id++)
			{
				for (flash_chip_ID_type chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
				{
					for (flash_channel_ID_type chan_id = 0; chan_id < channel_count; chan_id++)
					{
						NVM::FlashMemory::Physical_Page_Address plane_address;
						plane_address.ChannelID = (chan_id + rand_chan) % channel_count;
						plane_address.ChipID = (chip_id + rand_chip) % chip_no_per_channel;
						plane_address.DieID = (die_id + rand_die) % die_no_per_chip;
						plane_address.PlaneID = (plane_id + rand_plane) % plane_no_per_die;
						PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);
						Check_data_migration_required(pbke->Get_free_slc_block_pool_size(), plane_address);
					}
				}
			}
		}
		Consecutive_dm_num++;
		if (block_manager->Is_data_migration_needed() == true && Consecutive_dm_num < 100)
		{
			Simulator->Register_sim_event(Simulator->Time()+500000000, this, NULL, 0);
		}
		else
		{
			Consecutive_dm_num = 0;
		}
		
	}

	void GC_and_WL_Unit_Page_Level::Check_tlc_gc_required(const unsigned int free_tlc_block_pool_size, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (free_tlc_block_pool_size < tlc_block_pool_gc_threshold)
		{
			flash_block_ID_type gc_candidate_block_id = block_manager->Get_coldest_tlc_block_id(plane_address);
			PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);

			if (pbke->Ongoing_erase_operations.size() >= max_ongoing_gc_reqs_per_plane)
				return;

			bool flag = false;
			switch (block_selection_policy)
			{
			case SSD_Components::GC_Block_Selection_Policy_Type::GREEDY://Find the set of blocks with maximum number of invalid pages and no free pages
			{
				gc_candidate_block_id = 0;
				for (flash_block_ID_type block_id = slc_block_no_per_plane; block_id < block_no_per_plane; block_id++)
				{
					if (pbke->Ongoing_erase_operations.find(block_id) != pbke->Ongoing_erase_operations.end())
						continue;
					else if (flag == false)
					{
						if (pbke->Blocks[block_id].Current_page_write_index == pages_no_per_tlc_block 
							&& is_safe_gc_wl_candidate(pbke, block_id) == true)
						{
							gc_candidate_block_id = block_id;
							flag = true;
						}
					}
					else
					{
						if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
							&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_tlc_block
							&& is_safe_gc_wl_candidate(pbke, block_id) == true)
						{
							gc_candidate_block_id = block_id;
						}
					}
				}
				if (flag == false)
					return;
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RGA:
			{
				std::set<flash_block_ID_type> random_set;
				while (random_set.size() < tlc_rga_set_size)
				{
					flash_block_ID_type block_id = random_generator.Uniform_uint(slc_block_no_per_plane, block_no_per_plane - 1);
					if (pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
						&& is_safe_gc_wl_candidate(pbke, block_id))
						random_set.insert(block_id);
				}
				gc_candidate_block_id = *random_set.begin();
				for(auto &block_id : random_set)
					if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
						&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block)
						gc_candidate_block_id = block_id;
				break;
			}
			//********************************
			/*
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;
				while (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id) && repeat++ < block_no_per_plane)//A write frontier block should not be selected for garbage collection
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_P:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//A write frontier block or a block with free pages should not be selected for garbage collection
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block || !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_PP:
			{
				gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				unsigned int repeat = 0;

				//The selected gc block should have a minimum number of invalid pages
				while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block 
					|| pbke->Blocks[gc_candidate_block_id].Invalid_page_count < random_pp_threshold
					|| !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
					&& repeat++ < block_no_per_plane)
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
				break;
			}
			case SSD_Components::GC_Block_Selection_Policy_Type::FIFO:
				gc_candidate_block_id = pbke->Block_usage_history.front();
				pbke->Block_usage_history.pop();
				break;
			*/
			default:
			{
				std::cout << "Wrong GC Policy..." << std::endl;
				break;
			}
			}
			
			if (pbke->Ongoing_erase_operations.find(gc_candidate_block_id) != pbke->Ongoing_erase_operations.end())//This should never happen, but we check it here for safty
				return;

			if (pbke->Blocks[gc_candidate_block_id].Has_ongoing_gc_wl == true)
				return;
			
			NVM::FlashMemory::Physical_Page_Address gc_candidate_address(plane_address);
			gc_candidate_address.BlockID = gc_candidate_block_id;
			Block_Pool_Slot_Type* block = &pbke->Blocks[gc_candidate_block_id];
			if (block->Current_page_write_index == 0 || block->Invalid_page_count == 0)//No invalid page to erase
				return;
			//Run the state machine to protect against race condition
			block_manager->GC_WL_started(gc_candidate_address);
			pbke->Ongoing_erase_operations.insert(gc_candidate_block_id);
			address_mapping_unit->Set_barrier_for_accessing_physical_block(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing

			pbke->Doing_garbage_collection = true;
			pbke->Triggered_garbage_collection++;
			if (pbke->Doing_data_migration == true)
			{
				pbke->Data_migration_should_be_terminated = true;
			}

			if (block_manager->Can_execute_gc_wl(gc_candidate_address))//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
			{
				Stats::Total_gc_executions++;
				Stats::Tlc_gc_executions++;
				tsu->Prepare_for_transaction_submit();
				NVM_Transaction_Flash_ER* gc_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[gc_candidate_block_id].Stream_id, gc_candidate_address);
				unsigned int valid_page_count = 0;
				if (block->Current_page_write_index - block->Invalid_page_count > 0)//If there are some valid pages in block, then prepare flash transactions for page movement
				{
					NVM_Transaction_Flash_RD* gc_read = NULL;
					NVM_Transaction_Flash_WR* gc_write = NULL;
					for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++)
					{
						if (block_manager->Is_page_valid(block, pageID))
						{
							valid_page_count++;
							Stats::Total_page_movements_for_gc++;
							Stats::Tlc_gc_page_movements++;
							gc_candidate_address.PageID = pageID;
							if (use_copyback)
							{
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
								tsu->Submit_transaction(gc_write);
							}
							else
							{
								gc_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), gc_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, NO_PPA, gc_candidate_address, NULL, 0, gc_read, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
								gc_write->RelatedErase = gc_erase_tr;
								gc_read->RelatedWrite = gc_write;
								tsu->Submit_transaction(gc_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
							}
							gc_erase_tr->Page_movement_activities.push_back(gc_write);
						}
					}
				}
				block->Erase_transaction = gc_erase_tr;
				tsu->Submit_transaction(gc_erase_tr);
				tsu->Schedule();
			}
		}
	}
}