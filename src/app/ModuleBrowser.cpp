#include "app.hpp"
#include "plugin.hpp"
#include "window.hpp"
#include <set>


#define BND_LABEL_FONT_SIZE 13


namespace rack {


static std::set<Model*> sFavoriteModels;


bool isMatch(std::string s, std::string search) {
	s = lowercase(s);
	search = lowercase(search);
	return (s.find(search) != std::string::npos);
}

static bool isModelMatch(Model *model, std::string search) {
	if (search.empty())
		return true;
	std::string s;
	s += model->plugin->slug;
	s += " ";
	s += model->manufacturer;
	s += " ";
	s += model->name;
	s += " ";
	s += model->slug;
	for (ModelTag tag : model->tags) {
		s += " ";
		s += gTagNames[tag];
	}
	return isMatch(s, search);
}


struct FavoriteRadioButton : RadioButton {
	Model *model = NULL;

	void onAction(EventAction &e) override;
};


struct SeparatorItem : OpaqueWidget {
	SeparatorItem() {
		box.size.y = BND_WIDGET_HEIGHT;
	}

	void setText(std::string text) {
		clearChildren();
		Label *label = Widget::create<Label>(Vec(0, 0));
		label->text = text;
		addChild(label);
	}
};


struct BrowserListItem : OpaqueWidget {
	bool selected = false;

	BrowserListItem() {
		box.size.y = 2 * BND_WIDGET_HEIGHT + 7;
	}

	void draw(NVGcontext *vg) override {
		BNDwidgetState state = selected ? BND_HOVER : BND_DEFAULT;
		bndMenuItem(vg, 0.0, 0.0, box.size.x, box.size.y, state, -1, "");
		Widget::draw(vg);
	}

	void onDragDrop(EventDragDrop &e) override {
		if (e.origin != this)
			return;
		doAction();
	}

	void doAction() {
		EventAction eAction;
		eAction.consumed = true;
		onAction(eAction);
		if (eAction.consumed) {
			// deletes `this`
			gScene->setOverlay(NULL);
		}
	}

	void onMouseEnter(EventMouseEnter &e) override;
};



struct ModelItem : BrowserListItem {
	Model *model;
	Label *manufacturerLabel;

	void setModel(Model *model) {
		clearChildren();
		assert(model);
		this->model = model;

		Label *nameLabel = Widget::create<Label>(Vec(0, 0));
		nameLabel->text = model->name;
		addChild(nameLabel);

		manufacturerLabel = Widget::create<Label>(Vec(0, 0));
		manufacturerLabel->alignment = Label::RIGHT_ALIGNMENT;
		manufacturerLabel->text = model->manufacturer;
		addChild(manufacturerLabel);

		SequentialLayout *layout2 = Widget::create<SequentialLayout>(Vec(7, BND_WIDGET_HEIGHT));
		layout2->spacing = 10;
		addChild(layout2);

		FavoriteRadioButton *favoriteButton = new FavoriteRadioButton();
		favoriteButton->box.size.x = 20;
		favoriteButton->label = "★";
		layout2->addChild(favoriteButton);

		auto it = sFavoriteModels.find(model);
		if (it != sFavoriteModels.end())
			favoriteButton->setValue(1);
		favoriteButton->model = model;

		// for (ModelTag tag : model->tags) {
		// 	Button *tagButton = new Button();
		// 	tagButton->box.size.x = 120;
		// 	tagButton->text = gTagNames[tag];
		// 	layout2->addChild(tagButton);
		// }
	}

	void step() override {
		BrowserListItem::step();
		manufacturerLabel->box.size.x = box.size.x - BND_SCROLLBAR_WIDTH;
	}

	void onAction(EventAction &e) override {
		ModuleWidget *moduleWidget = model->createModuleWidget();
		gRackWidget->moduleContainer->addChild(moduleWidget);
		// Move module nearest to the mouse position
		moduleWidget->box.pos = gRackWidget->lastMousePos.minus(moduleWidget->box.size.div(2));
		gRackWidget->requestModuleBoxNearest(moduleWidget, moduleWidget->box);
	}
};


struct ManufacturerItem : BrowserListItem {
	std::string manufacturer;
	Label *manufacturerLabel;

	ManufacturerItem() {
	}

	void setManufacturer(std::string manufacturer) {
		clearChildren();
		this->manufacturer = manufacturer;
		manufacturerLabel = Widget::create<Label>(Vec(0, 0));
		if (manufacturer.empty())
			manufacturerLabel->text = "Show all modules";
		else
			manufacturerLabel->text = manufacturer;
		addChild(manufacturerLabel);
	}

	void step() override {
		BrowserListItem::step();
	}

	void onAction(EventAction &e) override;
};


struct BrowserList : List {
	int selected = 0;

	void step() override {
		// If we have zero children, this result doesn't matter anyway.
		selected = clamp(selected, 0, children.size() - 1);
		int i = 0;
		for (Widget *child : children) {
			BrowserListItem *item = dynamic_cast<BrowserListItem*>(child);
			if (item) {
				item->selected = (i == selected);
				i++;
			}
		}
		List::step();
	}

	void selectItem(Widget *w) {
		int i = 0;
		for (Widget *child : children) {
			BrowserListItem *item = dynamic_cast<BrowserListItem*>(child);
			if (item) {
				if (child == w) {
					selected = i;
					break;
				}
				i++;
			}
		}
	}

	BrowserListItem *getSelectedItem() {
		int i = 0;
		for (Widget *child : children) {
			BrowserListItem *item = dynamic_cast<BrowserListItem*>(child);
			if (item) {
				if (i == selected) {
					return item;
				}
				i++;
			}
		}
		return NULL;
	}
};


struct ModuleBrowser;

struct SearchModuleField : TextField {
	ModuleBrowser *moduleBrowser;
	void onTextChange() override;
	void onKey(EventKey &e) override;
};


struct ModuleBrowser : OpaqueWidget {
	SearchModuleField *searchField;
	ScrollWidget *moduleScroll;
	BrowserList *moduleList;
	std::string manufacturerFilter;

	ModuleBrowser() {
		box.size.x = 400;

		// Search
		searchField	= new SearchModuleField();
		searchField->box.size.x = box.size.x;
		searchField->moduleBrowser = this;
		addChild(searchField);

		moduleList = new BrowserList();
		moduleList->box.size = Vec(box.size.x, 0.0);

		// Module Scroll
		moduleScroll = new ScrollWidget();
		moduleScroll->box.pos.y = searchField->box.size.y;
		moduleScroll->box.size.x = box.size.x;
		moduleScroll->container->addChild(moduleList);
		addChild(moduleScroll);

		// Trigger search update
		searchField->setText("");
	}

	void refreshSearch() {
		std::string search = searchField->text;
		moduleList->clearChildren();
		moduleList->selected = 0;

		// Favorites
		{
			SeparatorItem *item = new SeparatorItem();
			item->setText("Favorites");
			moduleList->addChild(item);
		}
		for (Model *model : sFavoriteModels) {
			if ((manufacturerFilter.empty() || manufacturerFilter == model->manufacturer) && isModelMatch(model, search)) {
				ModelItem *item = new ModelItem();
				item->setModel(model);
				moduleList->addChild(item);
			}
		}

		// Manufacturers
		{
			SeparatorItem *item = new SeparatorItem();
			item->setText("Manufacturers");
			moduleList->addChild(item);
		}
		if (manufacturerFilter.empty()) {
			// Collect all manufacturers
			std::set<std::string> manufacturers;
			for (Plugin *plugin : gPlugins) {
				for (Model *model : plugin->models) {
					if (model->manufacturer.empty())
						continue;
					manufacturers.insert(model->manufacturer);
				}
			}
			// Manufacturer items
			for (std::string manufacturer : manufacturers) {
				if (isMatch(manufacturer, search)) {
					ManufacturerItem *item = new ManufacturerItem();
					item->setManufacturer(manufacturer);
					moduleList->addChild(item);
				}
			}
		}
		else {
			// Dummy manufacturer for clearing manufacturer filter
			ManufacturerItem *item = new ManufacturerItem();
				item->setManufacturer("");
			moduleList->addChild(item);
		}

		// Models
		{
			SeparatorItem *item = new SeparatorItem();
			item->setText("Modules");
			moduleList->addChild(item);
		}
		for (Plugin *plugin : gPlugins) {
			for (Model *model : plugin->models) {
				if ((manufacturerFilter.empty() || manufacturerFilter == model->manufacturer) && isModelMatch(model, search)) {
					ModelItem *item = new ModelItem();
					item->setModel(model);
					moduleList->addChild(item);
				}
			}
		}
	}

	void step() override {
		box.pos = parent->box.size.minus(box.size).div(2).round();
		box.pos.y = 60;
		box.size.y = parent->box.size.y - 2 * box.pos.y;

		moduleScroll->box.size.y = box.size.y - moduleScroll->box.pos.y;
		gFocusedWidget = searchField;
		Widget::step();
	}
};


// Implementations of inline methods above

void ManufacturerItem::onAction(EventAction &e) {
	ModuleBrowser *moduleBrowser = getAncestorOfType<ModuleBrowser>();
	moduleBrowser->manufacturerFilter = manufacturer;
	moduleBrowser->refreshSearch();
	e.consumed = false;
}

void FavoriteRadioButton::onAction(EventAction &e) {
	if (!model)
		return;
	if (value) {
		sFavoriteModels.insert(model);
	}
	else {
		auto it = sFavoriteModels.find(model);
		if (it != sFavoriteModels.end())
			sFavoriteModels.erase(it);
	}

	ModuleBrowser *moduleBrowser = getAncestorOfType<ModuleBrowser>();
	if (moduleBrowser)
		moduleBrowser->refreshSearch();
}

void BrowserListItem::onMouseEnter(EventMouseEnter &e) {
	BrowserList *list = getAncestorOfType<BrowserList>();
	list->selectItem(this);
}

void SearchModuleField::onTextChange() {
	moduleBrowser->refreshSearch();
}

void SearchModuleField::onKey(EventKey &e) {
	switch (e.key) {
		case GLFW_KEY_ESCAPE: {
			gScene->setOverlay(NULL);
			e.consumed = true;
			return;
		} break;
		case GLFW_KEY_UP: {
			moduleBrowser->moduleList->selected--;
			e.consumed = true;
		} break;
		case GLFW_KEY_DOWN: {
			moduleBrowser->moduleList->selected++;
			e.consumed = true;
		} break;
		case GLFW_KEY_ENTER: {
			BrowserListItem *item = moduleBrowser->moduleList->getSelectedItem();
			if (item) {
				item->doAction();
				e.consumed = true;
				return;
			}
		} break;
	}

	if (!e.consumed) {
		TextField::onKey(e);
	}
}

// Global functions

void appModuleBrowserCreate() {
	MenuOverlay *overlay = new MenuOverlay();

	ModuleBrowser *moduleBrowser = new ModuleBrowser();
	overlay->addChild(moduleBrowser);
	gScene->setOverlay(overlay);
}

json_t *appModuleBrowserToJson() {
	json_t *rootJ = json_object();

	json_t *favoritesJ = json_array();
	for (Model *model : sFavoriteModels) {
		json_t *modelJ = json_object();
		json_object_set_new(modelJ, "plugin", json_string(model->plugin->slug.c_str()));
		json_object_set_new(modelJ, "model", json_string(model->slug.c_str()));
		json_array_append_new(favoritesJ, modelJ);
	}
	json_object_set_new(rootJ, "favorites", favoritesJ);

	return rootJ;
}

void appModuleBrowserFromJson(json_t *rootJ) {
	json_t *favoritesJ = json_object_get(rootJ, "favorites");
	if (favoritesJ) {
		size_t i;
		json_t *favoriteJ;
		json_array_foreach(favoritesJ, i, favoriteJ) {
			json_t *pluginJ = json_object_get(favoriteJ, "plugin");
			json_t *modelJ = json_object_get(favoriteJ, "model");
			if (!pluginJ || !modelJ)
				continue;
			std::string pluginSlug = json_string_value(pluginJ);
			std::string modelSlug = json_string_value(modelJ);
			Model *model = pluginGetModel(pluginSlug, modelSlug);
			if (!model)
				continue;
			sFavoriteModels.insert(model);
		}
	}
}


} // namespace rack
