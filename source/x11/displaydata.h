#ifndef __DISPLAY_DATA_H__
#define __DISPLAY_DATA_H__

#include "../util/bheap.h"

#define DisplayDataPtr std::shared_ptr<DisplayData>
#define ContextDataPtr std::shared_ptr<ContextData>

class ContextData {
public:
	bool get(U32 contextType, U32& result) {
		return data.get(contextType, result);
	}
	void put(U32 contextType, U32 ptr) {
		data.set(contextType, ptr);
	}
	bool remove(U32 contextType) {
		U32 prev;

		if (data.get(contextType, prev)) {
			data.remove(contextType);
			return true;
		}
		return false;
	}
private:
	BHashTable<U32, U32> data;
};

class DisplayData {
public:
	void putEvent(const XEvent& event);
	U32 lockEvents();
	XEvent* getEvent(U32 index); // only call between lockEvents/unlockEvents
	void removeEvent(U32 index); // only call between lockEvents/unlockEvents
	void unlockEvents();
		
	U32 getEventMask(U32 window);
	void setEventMask(U32 window, U32 mask);

	U32 getNextEventSerial();	
	
	int getContextData(U32 context, U32 contextType, U32& ptr);
	int setContextData(U32 context, U32 contextType, U32 ptr);
	int deleteContextData(U32 context, U32 contextType);

	U32 displayAddress;
	U32 displayId;
	U32 root;

	BOXEDWINE_MUTEX mutex;
private:	
	std::atomic_int nextEventSerial;

	BOXEDWINE_MUTEX eventMutex;
	std::deque<XEvent> eventQueue;

	BOXEDWINE_MUTEX eventMaskMutex;
	BHashTable<U32, U32> perWindowEventMask;

	BOXEDWINE_MUTEX contextMutex;
	BHashTable<U32, ContextDataPtr> contextData;
};

#endif