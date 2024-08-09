#include "boxedwine.h"
#include "x11.h"

void XVisualInfo::read(KMemory* memory, U32 address) {
	this->visual = memory->readd(address); address += 4;
	visualid = memory->readd(address); address += 4;
	screen = (S32)memory->readd(address); address += 4;
	depth = (S32)memory->readd(address); address += 4;
	c_class = (S32)memory->readd(address); address += 4;
	red_mask = memory->readd(address); address += 4;
	green_mask = memory->readd(address); address += 4;
	blue_mask = memory->readd(address); address += 4;
	colormap_size = (S32)memory->readd(address); address += 4;
	bits_per_rgb = (S32)memory->readd(address);
}

bool XVisualInfo::match(U32 mask, S32 screenIndex, const Depth* depth, const Visual* visual) {
	if ((mask & VisualIDMask) && this->visualid != visual->visualid) {
		return false;
	}
	if ((mask & VisualScreenMask) && this->screen != screenIndex) {
		return false;
	}
	if ((mask & VisualDepthMask) && this->depth != depth->depth) {
		return false;
	}
	if ((mask & VisualClassMask) && this->c_class != visual->c_class) {
		return false;
	}
	if ((mask & VisualRedMaskMask) && this->red_mask != visual->red_mask) {
		return false;
	}
	if ((mask & VisualGreenMaskMask) && this->green_mask != visual->green_mask) {
		return false;
	}
	if ((mask & VisualBlueMaskMask) && this->blue_mask != visual->blue_mask) {
		return false;
	}
	if ((mask & VisualColormapSizeMask) && this->colormap_size != visual->map_entries) {
		return false;
	}
	if ((mask & VisualBitsPerRGBMask) && this->bits_per_rgb != visual->bits_per_rgb) {
		return false;
	}
	return true;
}

void XVisualInfo::set(S32 screenIndex, U32 visualAddress, Depth* depth, Visual* visual) {
	this->visual = visualAddress;
	this->visualid = visual->visualid;
	this->depth = depth->depth;
	this->c_class = visual->c_class;
	this->red_mask = visual->red_mask;
	this->green_mask = visual->green_mask;
	this->blue_mask = visual->blue_mask;
	this->colormap_size = visual->map_entries;
	this->bits_per_rgb = visual->bits_per_rgb;
}