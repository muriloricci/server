//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "container.h"
#include "game.h"
#include "player.h"

extern Game g_game;

Container::Container(const uint16_t _type) : Item(_type)
{
	//std::cout << "Container constructor " << this << std::endl;
	maxSize = items[this->getID()].maxItems;
	depotId = 0;
}

Container::~Container()
{
	//std::cout << "Container destructor " << this << std::endl;
	for(ItemList::iterator cit = itemlist.begin(); cit != itemlist.end(); ++cit){
		
		(*cit)->setParent(NULL);
		(*cit)->releaseThing2();
	}
    
	itemlist.clear();
}

double Container::getWeight() const
{
	double weight = items[id].weight;
	std::list<const Container*> stack;

	ItemList::const_iterator it;
	stack.push_back(this);
	
	while(stack.size() > 0) {
		const Container* container = stack.front();
		stack.pop_front();

		for (it = container->getItems(); it != container->getEnd(); ++it) {
			Container* container = dynamic_cast<Container*>(*it);
			if(container) {
				stack.push_back(container);
				weight += items[container->getID()].weight;
			}
			else
				weight += (*it)->getWeight();
		}
	}

	return weight;
}

ItemList::const_iterator Container::getItems() const
{
	return itemlist.begin();
}

ItemList::const_iterator Container::getEnd() const
{
	return itemlist.end();
}

Item* Container::getItem(uint32_t index)
{
	size_t n = 0;			
	for (ItemList::const_iterator cit = getItems(); cit != getEnd(); ++cit) {
		if(n == index)
			return *cit;
		else
			++n;
	}

	return NULL;
}

uint32_t Container::getItemHoldingCount() const
{
	uint32_t holdcount = 0;

	std::list<const Container*> stack;
	stack.push_back(this);
	
	ItemList::const_iterator it;

	while(stack.size() > 0) {
		const Container *container = stack.front();
		stack.pop_front();

		for (it = container->getItems(); it != container->getEnd(); ++it) {
			Container *container = dynamic_cast<Container*>(*it);
			if(container) {
				stack.push_back(container);
			}

			++holdcount;
		}
	}

	return holdcount;
}

bool Container::isHoldingItem(const Item* item) const
{
	std::list<const Container*> stack;
	stack.push_back(this);
	
	ItemList::const_iterator it;

	while(stack.size() > 0) {
		const Container *container = stack.front();
		stack.pop_front();

		for (it = container->getItems(); it != container->getEnd(); ++it) {

			if(*it == item) {
				return true;
			}

			Container *containerIt = dynamic_cast<Container*>(*it);
			if(containerIt){
				stack.push_back(containerIt);
			}
		}
	}

	return false;
}

ReturnValue Container::__moveThingTo(Creature* creature, Cylinder* toCylinder, uint32_t index, Thing* thing, uint32_t count)
{
	Item* item = dynamic_cast<Item*>(thing);
	if(item == NULL)
		return RET_NOTPOSSIBLE;

	//check constraints before moving
	//
	//
	//
	
	//check how much we really can move (count)
	//
	//

	if(item->isStackable()) {
		uint32_t fromIndex = __getIndexOfThing(item);

		ReturnValue ret = __removeThing(item, count);
		if(ret != RET_NOERROR)
			return ret;

		//Special condition if we are moving a stackable item in the same container
		//index will not be correct after removing "fromItem" if the fromIndex is < index
		if(this == toCylinder && count == item->getItemCountOrSubtype()){
			if(fromIndex < index)
				index = index - 1;
		}

		Item* moveItem = Item::CreateItem(item->getID(), count);
		ret = toCylinder->__addThing(index, moveItem);

		//todo: Fix leak here

		if(item->getItemCountOrSubtype() == 0){
			g_game.FreeThing(item);
		}

		if(ret != RET_NOERROR)
			return ret;
	}
	else{
		ReturnValue ret = __removeThing(item);

		if(ret != RET_NOERROR)
			return ret;

		ret = toCylinder->__addThing(index, item);

		if(ret != RET_NOERROR)
			return ret;
	}

	return RET_NOERROR;
}

/*ReturnValue Container::__queryCanMove(uint32_t index, Thing* thing, uint32_t inCount, uint32_t& outCount)
{
	return RET_NOTPOSSIBLE;
}*/

ReturnValue Container::__addThing(Thing* thing)
{
	Item* item = dynamic_cast<Item*>(thing);
	if(item == NULL)
		return RET_NOTPOSSIBLE;

	return RET_NOERROR;
}

ReturnValue Container::__addThing(uint32_t index, Thing* thing)
{
	if(index < 0 || index >= capacity())
		return RET_NOTPOSSIBLE;

	Item* item = dynamic_cast<Item*>(thing);
	if(item == NULL)
		return RET_NOTPOSSIBLE;

	if(itemlist.size() >= capacity())
		return RET_NOTENOUGHROOM;

	const Position& cylinderMapPos = getPosition();

	SpectatorVec list;
	SpectatorVec::iterator it;
	g_game.getSpectators(Range(cylinderMapPos, 2, 2, 2, 2, false), list);

	Item* toItem = getItem(index);
	if(toItem){
		if(toItem->isStackable() && toItem->getID() == item->getID()){
			uint32_t oldToCount = toItem->getItemCountOrSubtype();
			uint32_t newToCount = std::min((uint32_t)100, oldToCount + item->getItemCountOrSubtype());
			toItem->setItemCountOrSubtype(newToCount);

			//send to client
			int remainCount = item->getItemCountOrSubtype() - (newToCount - oldToCount);			
			item->setItemCountOrSubtype(remainCount);

			for(it = list.begin(); it != list.end(); ++it) {
				Player* spectator = dynamic_cast<Player*>(*it);
				if(spectator){
					spectator->sendUpdateContainerItem(this, index, toItem);
				}
			}

			if(remainCount != 0){
				if(itemlist.size() >= capacity())
					return RET_NOTENOUGHROOM;

				item->setParent(this);
				itemlist.push_front(item);

				//send to client
				for(it = list.begin(); it != list.end(); ++it) {
					Player* spectator = dynamic_cast<Player*>(*it);
					if(spectator){
						spectator->sendAddContainerItem(this, item);
					}
				}
			}
			
			return RET_NOERROR;
		}

		return RET_NOTENOUGHROOM;
	}
	else{
		item->setParent(this);
		itemlist.push_front(item);

		//send to client
		for(it = list.begin(); it != list.end(); ++it) {
			Player* spectator = dynamic_cast<Player*>(*it);
			if(spectator){
				spectator->sendAddContainerItem(this, item);
			}
		}
	}

	return RET_NOERROR;
}

ReturnValue Container::__updateThing(Thing* thing)
{
	return RET_NOTPOSSIBLE;
}

ReturnValue Container::__updateThing(uint32_t index, Thing* thing)
{
	return RET_NOTPOSSIBLE;
}

ReturnValue Container::__removeThing(Thing* thing)
{
	return RET_NOTPOSSIBLE;
}

ReturnValue Container::__removeThing(Thing* thing, uint32_t count)
{
	uint32_t index = __getIndexOfThing(thing);
	if(index == -1){
#ifdef __DEBUG__
		std::cout << "Failure: [Container::__removeThing] item == NULL" << std::endl;
#endif
		return RET_NOTPOSSIBLE;
	}

	ItemList::iterator cit = std::find(itemlist.begin(), itemlist.end(), thing);
	if(cit == itemlist.end())
		return RET_NOTPOSSIBLE;

	const Position& cylinderMapPos = getPosition();

	SpectatorVec list;
	SpectatorVec::iterator it;
	g_game.getSpectators(Range(cylinderMapPos, 2, 2, 2, 2, false), list);

	Item* item = *cit;
	if(item->isStackable()) {
		if(count == 0 || count > item->getItemCountOrSubtype())
			return RET_NOTPOSSIBLE;

		if(count == item->getItemCountOrSubtype()){
			(*cit)->setParent(NULL);
			itemlist.erase(cit);

			//send change to client
			for(it = list.begin(); it != list.end(); ++it) {
				Player* spectator = dynamic_cast<Player*>(*it);
				if(spectator){
					spectator->sendRemoveContainerItem(this, index);
				}
			}

			return RET_NOERROR;
		}
		else{
			item->setItemCountOrSubtype(item->getItemCountOrSubtype() - count);

			//send change to client
			for(it = list.begin(); it != list.end(); ++it) {
				Player* spectator = dynamic_cast<Player*>(*it);
				if(spectator){
					spectator->sendUpdateContainerItem(this, index, item);
				}
			}

			return RET_NOERROR;
		}
	}
	else{
		(*cit)->setParent(NULL);
		itemlist.erase(cit);

		//send change to client
		for(it = list.begin(); it != list.end(); ++it) {
			Player* spectator = dynamic_cast<Player*>(*it);
			if(spectator){
				spectator->sendRemoveContainerItem(this, index);
			}
		}

		return RET_NOERROR;
	}
	
	return RET_NOTPOSSIBLE;
}

uint32_t Container::__getIndexOfThing(const Thing* thing) const
{
	uint32_t index = 0;
	for (ItemList::const_iterator cit = getItems(); cit != getEnd(); ++cit) {
		if(*cit == thing)
			return index;
		else
			++index;
	}

	return -1;
}

void Container::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Container::__internalAddThing(uint32_t index, Thing* thing)
{
#ifdef __DEBUG__
	std::cout << "[Container::__internalAddThing] index: " << index << std::endl;
#endif

	Item* item = dynamic_cast<Item*>(thing);
	if(item == NULL){
#ifdef __DEBUG__
		std::cout << "Failure: [Container::__internalAddThing] item == NULL" << std::endl;
#endif
		return;
	}

	if(index < 0 || index >= capacity()){
#ifdef __DEBUG__
		std::cout << "Failure: [Container::__internalAddThing] - index is out of range" << std::endl;
#endif
		return;
	}

	itemlist.push_front(item);
	item->setParent(this);
}

/*
bool Container::addItem(Item *newitem) {
	//first check if we are a container, there is an item to be added and if we can add more items...
	//if (!iscontainer) throw TE_NoContainer();
	if (newitem == NULL)
		return false;

	// seems we should add the item...
	// new items just get placed in front of the items we already have...
	if(lcontained.size() < maxitems) {
		newitem->pos.x = 0xFFFF;

		//FIXME: is this correct? i dont get what it does. tliff
		Container* container = dynamic_cast<Container*>(newitem); 
		if(container) {
			container->setParent(this);
		}

		lcontained.push_front(newitem);

		// increase the itemcount
		++actualitems;
		return true;
	}

	return false;
}

bool Container::removeItem(Item* item)
{
	for (ItemList::iterator cit = lcontained.begin(); cit != lcontained.end(); cit++) {
		if((*cit) == item) {

			Container* container = dynamic_cast<Container*>(*cit); 
			if(container) {
				container->setParent(NULL);
			}

			lcontained.erase(cit);
			--actualitems;
			return true;
		}
	}

	return false;
}

void Container::moveItem(unsigned char from_slot, unsigned char to_slot)
{
	int n = 0;
	for (ItemList::iterator cit = lcontained.begin(); cit != lcontained.end(); ++cit) {
		if(n == from_slot) {
			Item *item = (*cit);
			lcontained.erase(cit);
			lcontained.push_front(item);
			break;
		}
		++n;
	}
}

const Item* Container::getItem(unsigned long slot_num) const
{
	size_t n = 0;			
	for (ItemList::const_iterator cit = getItems(); cit != getEnd(); ++cit) {
		if(n == slot_num)
			return *cit;
		else
			++n;
	}

	return NULL;
}
*/

/*
Container *Container::getTopParent()
{
	if(getParent() == NULL)
		return this;

	Container *aux = this->getParent();
	while(aux->getParent() != NULL) {
		aux = aux->getParent();
	}
	return aux;
}

const Container *Container::getTopParent() const
{
	if(getParent() == NULL)
		return this;

	Container *aux = this->getParent();

	while(aux->getParent() != NULL) {
		aux = aux->getParent();
	}
	return aux;
}
*/
