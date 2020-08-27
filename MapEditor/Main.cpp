#include <Siv3D.hpp> // OpenSiv3D v0.4.3

struct Object {
	int x, y;
	String Type, Param;

	template <class Archive>
	void SIV3D_SERIALIZE(Archive& archive)
	{
		archive(x, y, Type, Param);
	}
};

struct State {
	Grid<Point> tilemap;
	Grid<Array<Object>> objects;

	State() :
		tilemap(1, 1, Point(0, 0)), objects(1, 1) {}

	int height() const {
		return tilemap.height();
	}
	int width() const {
		return tilemap.width();
	}

	void HExpand() {
		tilemap.push_back_row(Point(0, 0));
		objects.push_back_row({});
	}
	void WExpand() {
		tilemap.push_back_column(Point(0, 0));
		objects.push_back_column({});
	}
	void HReduce() {
		tilemap.pop_back_row();
		objects.pop_back_row();
	}
	void WReduce() {
		tilemap.pop_back_column();
		objects.pop_back_column();
	}

	template <class Archive>
	void SIV3D_SERIALIZE(Archive& archive)
	{
		archive(tilemap, objects);
	}
};

void Main()
{
	Window::SetStyle(WindowStyle::Sizable);
	Scene::SetScaleMode(ScaleMode::ResizeFill);
	Scene::SetBackground(Color(80, 60, 70));
	Scene::SetTextureFilter(TextureFilter::Nearest);

	TextEditState tileSzTb, ObjTypeTb, ObjParamTb;
	tileSzTb.text = U"16";
	int tileSz = 16;

	Point selectedTile{ 0,0 };
	Optional<Point> gripStart{};

	Texture tex;

	double scaleRaw = 2;
	int scale = 2;

	Array<String> tools{ U"Tile Pen", U"Tile Rect", U"Object Pen", U"Object Erase" };
	size_t toolIndex = 0;

	ScopedRenderStates2D sampler(SamplerState::ClampNearest);

	State mapData;
	Array<State> history;

	Point camera = {};

	Font font(12);

	Optional<FilePath> path;

	while (System::Update())
	{
		auto historyLengthOld = history.size();

		bool GUIActive = tileSzTb.active || ObjTypeTb.active || ObjParamTb.active;

		if (toolIndex < 2) {
			SimpleGUI::Headline(U"TileSize", Vec2(10, 10));
			SimpleGUI::TextBox(tileSzTb, Vec2(10, 50), 100.0, 4);
			tileSz = ParseOr<int32>(tileSzTb.text, 16);

			SimpleGUI::Slider(U"Scale", scaleRaw, 1.0, 8.0, Vec2(10, 100));
			scale = Ceil(scaleRaw);
		}
		else
		{
			SimpleGUI::Headline(U"Object Type", Vec2(20, 10));
			SimpleGUI::TextBox(ObjTypeTb, Vec2(10, 50));

			SimpleGUI::Headline(U"Object Param", Vec2(20, 90));
			SimpleGUI::TextBox(ObjParamTb, Vec2(10, 130));
		}

		SimpleGUI::RadioButtons(toolIndex, tools, Vec2(220, 10));

		if (DragDrop::HasNewFilePaths()) {
			Texture tmpTex(DragDrop::GetDroppedFilePaths().front().path);
			if (tmpTex) {
				tex = tmpTex;
			}
		}

		if (tex)
		{
			{
				Transformer2D trans(Mat3x2::Translate(400, 0), Mat3x2::Translate(400, 0));

				tex.scaled(Ceil(scale)).draw(0, 0);

				for (int x = 0; x * tileSz < tex.width(); x++) {
					for (int y = 0; y * tileSz < tex.height(); y++) {
						auto rect = Rect(x * scale * tileSz, y * scale * tileSz, scale * tileSz, scale * tileSz);

						rect.drawFrame();

						if (rect.mouseOver()) {
							rect.draw(Color(Palette::White, 128));

							if (rect.leftClicked()) {
								selectedTile = Point(x, y);
							}
						}

						if (Point(x, y) == selectedTile) {
							rect.draw(Color(Palette::Yellow, 64));
						}
					}
				}
			}

			int bottom = Max(200, scale * tex.height() + 20);

			if (!GUIActive && !KeyControl.pressed())
			{
				if (KeyW.down() && mapData.height() > 1) {
					history.push_back(mapData);
					mapData.HReduce();
				}
				if (KeyS.down()) {
					history.push_back(mapData);
					mapData.HExpand();
				}
				if (KeyA.down() && mapData.width() > 1) {
					history.push_back(mapData);
					mapData.WReduce();
				}
				if (KeyD.down()) {
					history.push_back(mapData);
					mapData.WExpand();
				}

				if (KeyUp.pressed()) {
					camera.y += 10;
				}
				if (KeyDown.pressed()) {
					camera.y -= 10;
				}
				if (KeyLeft.pressed()) {
					camera.x += 10;
				}
				if (KeyRight.pressed()) {
					camera.x -= 10;
				}
			}

			{
				ScopedViewport2D viewport(0, bottom, Scene::Width(), Scene::Height() - bottom);
				Transformer2D trans(Mat3x2::Translate(camera),
					Mat3x2::Translate(camera) * Mat3x2::Translate(0, bottom));

				Optional<Point> cursor;

				for (int x = 0; x < mapData.width(); x++) {
					for (int y = 0; y < mapData.height(); y++) {
						auto rect = Rect(x * scale * tileSz, y * scale * tileSz, scale * tileSz, scale * tileSz);
						if (rect.mouseOver()) {
							cursor = Point{ x, y };
							break;
						}
					}
				}
				for (int x = 0; x < mapData.width(); x++) {
					for (int y = 0; y < mapData.height(); y++) {
						auto type = mapData.tilemap.at(y, x);

						auto rect = Rect(x * scale * tileSz, y * scale * tileSz, scale * tileSz, scale * tileSz);

						auto tileToDraw = type;

						switch (toolIndex)
						{
						case 0:	// tile pen
							if (rect.mouseOver()) {
								tileToDraw = selectedTile;
								if (rect.leftClicked()) {
									history.push_back(mapData);
								}
								if (rect.leftPressed()) {
									mapData.tilemap.at(y, x) = selectedTile;
								}
							}
							break;
						case 1:	// tile rect
							if (rect.leftClicked())
							{
								history.push_back(mapData);
								gripStart = Point{ x, y };
							}
							if (gripStart)
							{
								Point lt = { Min(gripStart->x, cursor->x), Min(gripStart->y, cursor->y) };
								Point rb = { Max(gripStart->x, cursor->x), Max(gripStart->y, cursor->y) };

								if (lt.x <= x && x <= rb.x &&
									lt.y <= y && y <= rb.y)
								{
									tileToDraw = selectedTile;
								}

								if (rect.leftReleased())
								{
									for (int rx = lt.x; rx <= rb.x; rx++) {
										for (int ry = lt.y; ry <= rb.y; ry++) {
											mapData.tilemap.at(ry, rx) = selectedTile;
										}
									}
								}
							}
							break;
						case 2:	// object pen
							if (rect.leftClicked())
							{
								history.push_back(mapData);

								Object obj;
								obj.x = x;
								obj.y = y;
								obj.Type = ObjTypeTb.text;
								obj.Param = ObjParamTb.text;
								mapData.objects.at(y, x).push_back(obj);
							}
							if (rect.rightClicked() && !mapData.objects.at(y, x).empty())
							{
								history.push_back(mapData);
								mapData.objects.at(y, x).pop_back();
							}
							break;
						case 3:	// object erase
							if (rect.leftClicked())
							{
								history.push_back(mapData);
								mapData.objects.at(y, x).clear();
							}
							break;
						}

						tex(tileToDraw.x * tileSz, tileToDraw.y * tileSz, tileSz, tileSz).scaled(scale)
							.draw(rect.pos);

						if (rect.mouseOver())
							rect.draw(Color(Palette::Yellow, 64));

						rect.drawFrame();

						auto& objs = mapData.objects.at(y, x);
						if (objs.size() > 0)
						{
							if (objs.size() == 1)
							{
								auto& type = objs.front().Type;
								font(type.empty() ? U"(null)" : type).draw(rect.pos, Palette::White);
							}
							else
							{
								font(U"x{}"_fmt(objs.size())).draw(rect.pos, Palette::Yellow);
							}
						}
					}
				}

				if(cursor)
				{
					String objText;
					auto& objDat = mapData.objects.at(*cursor);

					if (objDat.size())
					{
						for (size_t i = 0; i < objDat.size(); i++)
						{
							objText += U"Type:{}\nParam:{}\n"_fmt(objDat[i].Type, objDat[i].Param);
						}

						objText.pop_back();
						
						auto drawStr = font(objText);
						auto drawStrRect = drawStr.boundingRect(Cursor::Pos());
						drawStrRect.x -= 5;
						drawStrRect.y -= 5;
						drawStrRect.w += 10;
						drawStrRect.h += 10;
						drawStrRect.draw(Color(0, 16, 0, 128));
						drawStrRect.drawFrame(1.0, Color(255));
						drawStr.draw(Cursor::Pos());
					}
				}
			}
		}

		if (history.size() != historyLengthOld)
		{
			Window::SetTitle((path ? *path : U"Untitled") + U" *");
		}

		if (!MouseL.pressed() && KeyControl.pressed() && KeyZ.down() && history.size() > 0) {
			mapData = history.back();
			history.pop_back();
		}
		if (!MouseL.pressed() && KeyControl.pressed() && KeyS.down()) {
			Optional<FilePath> SavePath = path;

			if (!SavePath || KeyShift.pressed()) {
				SavePath = Dialog::SaveFile();
			}
			if (SavePath) {
				BinaryWriter w{ *SavePath };
				if (!w)
				{
					throw Error(U"Failed to open file");
				}
				w.clear();

				Serializer<BinaryWriter> writer(w);
				if (!writer.getWriter())
				{
					throw Error(U"Failed to open file");
				}

				writer(mapData);

				path = SavePath;

				Window::SetTitle(*path);
			}
		}

		if (!MouseL.pressed() && KeyControl.pressed() && KeyO.down()) {
			auto newPath = Dialog::OpenFile();
			if (newPath) {
				history.clear();
				path = newPath;

				Deserializer<BinaryReader> reader{ *path };

				if (!reader.getReader())
				{
					throw Error(U"Failed to open file");
				}
				reader(mapData);

				Window::SetTitle(*path);
			}
		}

		if (!MouseL.pressed() && KeyControl.pressed() && KeyE.down()) {
			auto savePath = Dialog::SaveFile({ FileFilter::JSON() });
			if (savePath)
			{
				JSONWriter json;

				json.startObject();
				{
					json.key(U"tilemap").startObject();
					{
						json.key(U"width").writeUint32(mapData.width());
						json.key(U"height").writeUint32(mapData.height());
						json.key(U"data").startArray();
						{
							for (size_t y = 0; y < mapData.height(); y++)
							{
								json.startArray();
								for (size_t x = 0; x < mapData.width(); x++)
								{
									json.startObject();

									auto& tile = mapData.tilemap.at(y, x);
									json.key(U"typeX").writeUint32(tile.x);
									json.key(U"typeY").writeUint32(tile.y);

									json.endObject();
								}
								json.endArray();
							}
						}
						json.endArray();
					}
					json.endObject();

					json.key(U"objects").startArray();
					{
						for (size_t y = 0; y < mapData.height(); y++)
						{
							for (size_t x = 0; x < mapData.width(); x++)
							{
								auto& objects = mapData.objects.at(y, x);
								for (size_t i = 0; i < objects.size(); i++)
								{
									json.startObject();

									json.key(U"X").writeUint32(objects[i].x);
									json.key(U"Y").writeUint32(objects[i].y);
									json.key(U"Type").writeString(objects[i].Type);
									json.key(U"Param").writeString(objects[i].Param);

									json.endObject();
								}
							}
						}
					}
					json.endArray();
				}
				json.endObject();

				json.save(*savePath);
			}
		}

		if (MouseL.up())
			gripStart.reset();

		while (history.size() > 1024) {
			history.pop_front();
		}
	}
}
