#include "TSU_OutofOrder.h"

namespace SSD_Components
{

	TSU_OutOfOrder::TSU_OutOfOrder(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, unsigned int ChannelCount, unsigned int chip_no_per_channel,
		unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite, 
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
		: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::OUT_OF_ORDER, ChannelCount, chip_no_per_channel, DieNoPerChip, PlaneNoPerDie,
			WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			EraseSuspensionEnabled, ProgramSuspensionEnabled)
	{
		UserReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		UserWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		GCEraseTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingReadTRQueue = new Flash_Transaction_Queue*[channel_count];
		MappingWriteTRQueue = new Flash_Transaction_Queue*[channel_count];
		queue_flag = false;
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			UserReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			UserWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			GCEraseTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			MappingWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
			{
				UserReadTRQueue[channelID][chip_cntr].Set_id("User_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				UserWriteTRQueue[channelID][chip_cntr].Set_id("User_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCReadTRQueue[channelID][chip_cntr].Set_id("GC_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingReadTRQueue[channelID][chip_cntr].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				MappingWriteTRQueue[channelID][chip_cntr].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCWriteTRQueue[channelID][chip_cntr].Set_id("GC_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
				GCEraseTRQueue[channelID][chip_cntr].Set_id("GC_Erase_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			}
		}
	}
	
	TSU_OutOfOrder::~TSU_OutOfOrder()
	{
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			delete[] UserReadTRQueue[channelID];
			delete[] UserWriteTRQueue[channelID];
			delete[] GCReadTRQueue[channelID];
			delete[] GCWriteTRQueue[channelID];
			delete[] GCEraseTRQueue[channelID];
			delete[] MappingReadTRQueue[channelID];
			delete[] MappingWriteTRQueue[channelID];
		}
		delete[] UserReadTRQueue;
		delete[] UserWriteTRQueue;
		delete[] GCReadTRQueue;
		delete[] GCWriteTRQueue;
		delete[] GCEraseTRQueue;
		delete[] MappingReadTRQueue;
		delete[] MappingWriteTRQueue;
	}

	void TSU_OutOfOrder::Start_simulation() {}

	void TSU_OutOfOrder::Validate_simulation_config() {}

	void TSU_OutOfOrder::Execute_simulator_event(MQSimEngine::Sim_Event* event) 
	{
		queue_flag = false;
		Serve_transactions();
	}

	void TSU_OutOfOrder::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
		name_prefix = name_prefix + +".TSU";
		xmlwriter.Write_open_tag(name_prefix);

		TSU_Base::Report_results_in_XML(name_prefix, xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				UserReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				UserWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				MappingReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				MappingWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Read_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Write_TR_Queue", xmlwriter);

		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
				GCEraseTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Erase_TR_Queue", xmlwriter);
	
		xmlwriter.Write_close_tag();
	}

	inline void TSU_OutOfOrder::Prepare_for_transaction_submit()
	{
		opened_scheduling_reqs++;
		if (opened_scheduling_reqs > 1)
			return;
		transaction_receive_slots.clear();
	}

	inline void TSU_OutOfOrder::Submit_transaction(NVM_Transaction_Flash* transaction)
	{
		transaction_receive_slots.push_back(transaction);
	}

	void TSU_OutOfOrder::Schedule()
	{
		opened_scheduling_reqs--;
		if (opened_scheduling_reqs > 0)
			return;
		if (opened_scheduling_reqs < 0)
			PRINT_ERROR("TSU_OutOfOrder: Illegal status!");

		if (transaction_receive_slots.size() == 0)
			return;

		for(std::list<NVM_Transaction_Flash*>::iterator it = transaction_receive_slots.begin();
			it != transaction_receive_slots.end(); it++)
			switch ((*it)->Type)
			{
			case Transaction_Type::READ:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::MAPPING:
					MappingReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_OutOfOrder: unknown source type for a read transaction!")
				}
				break;
			case Transaction_Type::WRITE:
				switch ((*it)->Source)
				{
				case Transaction_Source_Type::CACHE:
				case Transaction_Source_Type::USERIO:
					UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::MAPPING:
					MappingWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				case Transaction_Source_Type::GC_WL:
					GCWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
					break;
				default:
					PRINT_ERROR("TSU_OutOfOrder: unknown source type for a write transaction!")
				}
				break;
			case Transaction_Type::ERASE:
				GCEraseTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			default:
				break;
			}
		Serve_transactions();
	}

	void TSU_OutOfOrder::Serve_transactions()
	{
		for (flash_channel_ID_type channelID = 0; channelID < channel_count; channelID++)
		{
			if (_NVMController->Get_channel_status(channelID) == BusChannelStatus::IDLE)
			{
				for (unsigned int i = 0; i < chip_no_per_channel; i++) {
					NVM::FlashMemory::Flash_Chip* chip = _NVMController->Get_chip(channelID, Round_robin_turn_of_channel[channelID]);
					//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
					if (!service_read_transaction(chip))
						if (!service_write_transaction(chip))
							service_erase_transaction(chip);
					Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(Round_robin_turn_of_channel[channelID] + 1) % chip_no_per_channel;
					if (_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
						break;
				}
			}
		}
		
		if (queues_are_empty() == false && queue_flag == true)
		{
			queue_flag = true;
			Simulator->Register_sim_event(Simulator->Time()+10000, this, NULL, 0);
		}
		
	}
	
	bool TSU_OutOfOrder::service_read_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

		if (MappingReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)//Flash transactions that are related to FTL mapping data have the highest priority
		{
			sourceQueue1 = &MappingReadTRQueue[chip->ChannelID][chip->ChipID];
			if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip) && GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
		}
		else if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
				if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		} 
		else //If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		{
			if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
				if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			}
			else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
					return false;
			else if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}

		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::WRITING:
			if (!programSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < writeReasonableSuspensionTimeForRead)
				return false;
			suspensionRequired = true;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForRead)
				return false;
			suspensionRequired = true;
		default:
			return false;
		}
		
		flash_die_ID_type dieID = sourceQueue1->front()->Address.DieID;
		flash_page_ID_type pageID = sourceQueue1->front()->Address.PageID;
		unsigned int planeVector = 0;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end();)
			{
				if ((*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
				{
					if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
					{
						(*it)->SuspendRequired = suspensionRequired;
						planeVector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						sourceQueue1->remove(it++);
						continue;
					}
				}
				it++;
			}

			if (sourceQueue2 != NULL && transaction_dispatch_slots.size() < plane_no_per_die)
				for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end();)
				{
					if ((*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
					{
						if (planeVector == 0 || (*it)->Address.PageID == pageID)//Check for identical pages when running multiplane command
						{
							(*it)->SuspendRequired = suspensionRequired;
							planeVector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							sourceQueue2->remove(it++);
							continue;
						}
					}
					it++;
				}

			if (transaction_dispatch_slots.size() > 0)
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}

		return true;
	}

	bool TSU_OutOfOrder::is_a_full_page_set(std::list<NVM_Transaction_Flash*> transaction_receive_slots, int numOfPagesInOneSet, bool page_set_flag)
	{
		if (transaction_receive_slots.size() == numOfPagesInOneSet)
		{
			return true;
		}
		if (page_set_flag == true)
		{
			return true;
		}
		
		sim_time_type now_time;
		now_time = Simulator->Time();
		for (auto it = transaction_receive_slots.begin(); it != transaction_receive_slots.end(); it++)
		{
			if (((*it)->Address.PageID + 1) % numOfPagesInOneSet == 0)
				return true;
			if (now_time - (*it)->Issue_time > 100000000)
				return true;
		}
		
		return false;
	}

	bool TSU_OutOfOrder::service_write_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;
		if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
		{
			if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				return false;
			else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}
		else //If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		{
			if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
				if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				{
					sourceQueue2 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
				}
			}
			else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
				sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			else return false;
		}


		bool suspensionRequired = false;
		ChipStatus cs = _NVMController->GetChipStatus(chip);
		switch (cs)
		{
		case ChipStatus::IDLE:
			break;
		case ChipStatus::ERASING:
			if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
				return false;
			if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForWrite)
				return false;
			suspensionRequired = true;
		default:
			return false;
		}

		std::vector<int> die_recorder(die_no_per_chip, 0);
		for (unsigned int i = 0; i < die_no_per_chip; i++)
			die_recorder[i] = 0;
		transaction_dispatch_slots.clear();
		for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end(); )
		{
			flash_die_ID_type dieID = (*it)->Address.DieID;
			if (die_recorder[dieID] == 1)
			{
				it++;
				continue;
			}
			flash_plane_ID_type planeID = (*it)->Address.PlaneID;
			flash_block_ID_type blockID = (*it)->Address.BlockID;
			if (chip->Get_Flash_Type_of_Block(blockID, planeID, dieID) == Flash_Technology_Type::SLC)
			{
				if(((NVM_Transaction_Flash_WR*)*it)->RelatedRead == NULL)
				{
					die_recorder[dieID] = 1;
					(*it)->SuspendRequired = suspensionRequired;
					//planeVector |= 1 << (*it)->Address.PlaneID;
					transaction_dispatch_slots.push_back(*it);
					sourceQueue1->remove(it++);
					_NVMController->Send_command_to_chip(transaction_dispatch_slots);
					
					transaction_dispatch_slots.clear();
					continue;
				}
				else{
					//std::cout << "LPA: " << (*it)->LPA << " cannot program due to related read " << Simulator->Time() << std::endl;
				}
			}
			else{
				//std::cout << "LPA: " << (*it)->LPA << " not an SLC write" << std::endl;
			}
			it++;
		}
		transaction_dispatch_slots.clear();
		if (sourceQueue2 != NULL)
		{
			for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end(); )
			{
				flash_die_ID_type dieID = (*it)->Address.DieID;
				if (die_recorder[dieID] == 1)
				{
					it++;
					continue;
				}
				flash_plane_ID_type planeID = (*it)->Address.PlaneID;
				flash_block_ID_type blockID = (*it)->Address.BlockID;
				if (chip->Get_Flash_Type_of_Block(blockID, planeID, dieID) == Flash_Technology_Type::SLC && ((NVM_Transaction_Flash_WR*)*it)->RelatedRead == NULL)
				{
					die_recorder[dieID] = 1;
					(*it)->SuspendRequired = suspensionRequired;
					//planeVector |= 1 << (*it)->Address.PlaneID;
					transaction_dispatch_slots.push_back(*it);
					sourceQueue2->remove(it++);
					_NVMController->Send_command_to_chip(transaction_dispatch_slots);
					transaction_dispatch_slots.clear();
					continue;
				}
				it++;
			}
		}
		transaction_dispatch_slots.clear();
		if (chip->flash_program_method == Flash_Program_Type::PAGEBYPAGE)
		{
			for (unsigned int i = 0; i < die_no_per_chip; i++)
			{
				if (die_recorder[i] == 1)
				{
					continue;
				}
				if (sourceQueue1 == NULL)	continue;
				else if(sourceQueue1->size() == 0)	continue;
				for (Flash_Transaction_Queue::iterator it1 = sourceQueue1->begin(); it1 != sourceQueue1->end(); it1++)
				{
					flash_die_ID_type dieID = (*it1)->Address.DieID;
					if (dieID != i)	continue;
					flash_plane_ID_type planeID = (*it1)->Address.PlaneID;
					flash_block_ID_type blockID = (*it1)->Address.BlockID;
					flash_page_ID_type pageID = (*it1)->Address.PageID;
					if (chip->Get_Flash_Type_of_Block(blockID, planeID, dieID) == Flash_Technology_Type::SLC)
						continue;
					(*it1)->SuspendRequired = suspensionRequired;
					transaction_dispatch_slots.push_back(*it1);
					_NVMController->Send_command_to_chip(transaction_dispatch_slots);
					die_recorder[i] = 1;
					sourceQueue1->remove(*it1);
					transaction_dispatch_slots.clear();
					break;
				}
			}
			for (unsigned int i = 0; i < die_no_per_chip; i++)
			{
				if (die_recorder[i] == 1)
				{
					continue;
				}
				if (sourceQueue2 == NULL)	continue;
				else if(sourceQueue2->size() == 0)	continue;
				for (Flash_Transaction_Queue::iterator it2 = sourceQueue2->begin(); it2 != sourceQueue2->end(); it2++)
				{
					flash_die_ID_type dieID = (*it2)->Address.DieID;
					if (dieID != i)	continue;
					flash_plane_ID_type planeID = (*it2)->Address.PlaneID;
					flash_block_ID_type blockID = (*it2)->Address.BlockID;
					flash_page_ID_type pageID = (*it2)->Address.PageID;
					if (chip->Get_Flash_Type_of_Block(blockID, planeID, dieID) == Flash_Technology_Type::SLC)
						continue;
					(*it2)->SuspendRequired = suspensionRequired;
					transaction_dispatch_slots.push_back(*it2);
					_NVMController->Send_command_to_chip(transaction_dispatch_slots);
					die_recorder[i] = 1;
					sourceQueue2->remove(*it2);
					transaction_dispatch_slots.clear();
					break;
				}
			}
			return true;
		}
		else if(chip->flash_program_method == Flash_Program_Type::ONESHOT)
		{
			for (unsigned int i = 0; i < die_no_per_chip; i++)
			{
				if (die_recorder[i] == 1)
				{
					continue;
				}
				bool page_set_flag = false;
				if (sourceQueue1 == NULL)	continue;
				else if(sourceQueue1->size() == 0)	continue;
				for (Flash_Transaction_Queue::iterator it1 = sourceQueue1->begin(); it1 != sourceQueue1->end(); it1++)
				{
					flash_die_ID_type dieID = (*it1)->Address.DieID;
					if (dieID != i)	continue;
					flash_plane_ID_type planeID = (*it1)->Address.PlaneID;
					flash_block_ID_type blockID = (*it1)->Address.BlockID;
					flash_page_ID_type pageID = (*it1)->Address.PageID;
					if (chip->Get_Flash_Type_of_Block(blockID, planeID, dieID) == Flash_Technology_Type::SLC)
						continue;
					for (Flash_Transaction_Queue::iterator it2 = sourceQueue1->begin(); it2 != sourceQueue1->end(); it2++)
					{
						if (((NVM_Transaction_Flash_WR*)*it2)->RelatedRead == NULL && (*it2)->Address.DieID == dieID && (*it2)->Address.PlaneID == planeID && (*it2)->Address.BlockID == blockID)
						{
							if ((*it2)->Address.PageID / 3 == pageID / 3)
							{
								(*it2)->SuspendRequired = suspensionRequired;
								transaction_dispatch_slots.push_back(*it2);
								continue;
							}
							else if ((*it2)->Address.PageID / 3 > pageID / 3)
							{
								page_set_flag = true;
								break;
							}
						}
					}
					if (sourceQueue2 != NULL && transaction_dispatch_slots.size() < 3 && page_set_flag == false)
					{
						for (Flash_Transaction_Queue::iterator it3 = sourceQueue2->begin(); it3 != sourceQueue2->end(); it3++)
						{
							if (((NVM_Transaction_Flash_WR*)*it3)->RelatedRead == NULL && (*it3)->Address.DieID == dieID && (*it3)->Address.PlaneID == planeID && (*it3)->Address.BlockID == blockID)
							{
								if ((*it3)->Address.PageID / 3 == pageID / 3)
								{
									(*it3)->SuspendRequired = suspensionRequired;
									transaction_dispatch_slots.push_back(*it3);
									continue;
								}
								else if ((*it3)->Address.PageID / 3 > pageID / 3)
								{
									page_set_flag = true;
									break;
								}
							}
						}
					}
					if (transaction_dispatch_slots.size() == 0)
						continue;
					if (is_a_full_page_set(transaction_dispatch_slots, 3, page_set_flag) == true)
					{
						_NVMController->Send_command_to_chip(transaction_dispatch_slots);
						die_recorder[i] = 1;
						unsigned int queue1_size = sourceQueue1->size();
						for (auto it = transaction_dispatch_slots.begin(); it != transaction_dispatch_slots.end(); it++)
						{
							if (sourceQueue1 != NULL)
							{
								sourceQueue1->remove(*it);
							}
							if (sourceQueue1->size() < queue1_size)
							{
								queue1_size = sourceQueue1->size();
								continue;
							}
							else// if (sourceQueue2 != NULL)
							{
								sourceQueue2->remove(*it);
							}
						}
						transaction_dispatch_slots.clear();
						break;
					}
					else
					{
						transaction_dispatch_slots.clear();
					}
				}
			}
			for (unsigned int i = 0; i < die_no_per_chip; i++)
			{
				if (die_recorder[i] == 1)
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			std::cout << "Wrong Program Method :: TSU_OutofOrder.cpp" << std::endl;
		}
	}

	bool TSU_OutOfOrder::service_erase_transaction(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
			return false;

		Flash_Transaction_Queue* source_queue = &GCEraseTRQueue[chip->ChannelID][chip->ChipID];
		if (source_queue->size() == 0)
			return false;

		flash_die_ID_type dieID = source_queue->front()->Address.DieID;
		unsigned int planeVector = 0;
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = source_queue->begin(); it != source_queue->end(); )
			{
				if (((NVM_Transaction_Flash_ER*)*it)->Page_movement_activities.size() == 0 && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
				{
					planeVector |= 1 << (*it)->Address.PlaneID;
					transaction_dispatch_slots.push_back(*it);
					source_queue->remove(it++);
				}
				it++;
			}
			if (transaction_dispatch_slots.size() > 0)
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			}
			transaction_dispatch_slots.clear();
			dieID = (dieID + 1) % die_no_per_chip;
		}
		return true;
	}

	void TSU_OutOfOrder::print_queue_depth()
	{
		unsigned int read_transaction_count = 0;
		unsigned int write_transaction_count = 0;
		unsigned int gc_read_transaction_count = 0;
		unsigned int gc_write_transaction_count = 0;
		unsigned int erase_transaction_count = 0;
		for(int chan_cnt = 0; chan_cnt < channel_count; chan_cnt++)
		{
			for(int chip_cnt = 0; chip_cnt < chip_no_per_channel; chip_cnt++)
			{
				read_transaction_count += UserReadTRQueue[chan_cnt][chip_cnt].size();
				write_transaction_count += UserWriteTRQueue[chan_cnt][chip_cnt].size();
				gc_read_transaction_count += GCReadTRQueue[chan_cnt][chip_cnt].size();
				gc_write_transaction_count += GCWriteTRQueue[chan_cnt][chip_cnt].size();
				erase_transaction_count += GCEraseTRQueue[chan_cnt][chip_cnt].size();
			}
		}
		std::cout << read_transaction_count << ", " << write_transaction_count << ", " << gc_read_transaction_count << ", " << gc_write_transaction_count << ", " << erase_transaction_count << std::endl;
	}

	bool TSU_OutOfOrder::queues_are_empty()
	{
		unsigned int read_transaction_count = 0;
		unsigned int write_transaction_count = 0;
		unsigned int gc_read_transaction_count = 0;
		unsigned int gc_write_transaction_count = 0;
		unsigned int erase_transaction_count = 0;
		for(int chan_cnt = 0; chan_cnt < channel_count; chan_cnt++)
		{
			for(int chip_cnt = 0; chip_cnt < chip_no_per_channel; chip_cnt++)
			{
				read_transaction_count += UserReadTRQueue[chan_cnt][chip_cnt].size();
				write_transaction_count += UserWriteTRQueue[chan_cnt][chip_cnt].size();
				gc_read_transaction_count += GCReadTRQueue[chan_cnt][chip_cnt].size();
				gc_write_transaction_count += GCWriteTRQueue[chan_cnt][chip_cnt].size();
				erase_transaction_count += GCEraseTRQueue[chan_cnt][chip_cnt].size();
			}
		}
		if (write_transaction_count == 0 && gc_write_transaction_count == 0)
			return true;
		else
		{
			return false;
		}
		
	}
}
