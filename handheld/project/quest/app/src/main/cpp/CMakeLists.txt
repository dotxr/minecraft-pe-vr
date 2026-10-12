# Native build for the Quest (arm64-v8a, OpenXR) port of Minecraft PE 0.6.1.
cmake_minimum_required(VERSION 3.18)
project(minecraftpe_quest CXX C)

# The engine code predates C++11; build it in the same dialect it was written for.
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(HANDHELD "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
get_filename_component(HANDHELD "${HANDHELD}" ABSOLUTE)

set(GAME_SOURCES
    ${HANDHELD}/src/main_quest.cpp
    ${HANDHELD}/src/platform/gles1_shim.cpp
    ${HANDHELD}/src/platform/vr/vr_input.cpp
    ${HANDHELD}/src/platform/audio/SoundSystemSL.cpp
    ${HANDHELD}/src/platform/input/Controller.cpp
    ${HANDHELD}/src/platform/input/Keyboard.cpp
    ${HANDHELD}/src/platform/input/Mouse.cpp
    ${HANDHELD}/src/platform/input/Multitouch.cpp
    ${HANDHELD}/src/platform/time.cpp
    ${HANDHELD}/src/platform/CThread.cpp
    ${HANDHELD}/src/NinecraftApp.cpp
    ${HANDHELD}/src/Performance.cpp
    ${HANDHELD}/src/SharedConstants.cpp
    ${HANDHELD}/src/client/IConfigListener.cpp
    ${HANDHELD}/src/client/Minecraft.cpp
    ${HANDHELD}/src/client/MouseHandler.cpp
    ${HANDHELD}/src/client/Options.cpp
    ${HANDHELD}/src/client/OptionsFile.cpp
    ${HANDHELD}/src/client/OptionStrings.cpp
    ${HANDHELD}/src/client/gamemode/GameMode.cpp
    ${HANDHELD}/src/client/gamemode/CreativeMode.cpp
    ${HANDHELD}/src/client/gamemode/SurvivalMode.cpp
    ${HANDHELD}/src/client/gui/components/Button.cpp
    ${HANDHELD}/src/client/gui/components/GuiElement.cpp
    ${HANDHELD}/src/client/gui/components/GuiElementContainer.cpp
    ${HANDHELD}/src/client/gui/components/ImageButton.cpp
    ${HANDHELD}/src/client/gui/components/ItemPane.cpp
    ${HANDHELD}/src/client/gui/components/InventoryPane.cpp
    ${HANDHELD}/src/client/gui/components/LargeImageButton.cpp
    ${HANDHELD}/src/client/gui/components/NinePatch.cpp
    ${HANDHELD}/src/client/gui/components/OptionsGroup.cpp
    ${HANDHELD}/src/client/gui/components/OptionsItem.cpp
    ${HANDHELD}/src/client/gui/components/OptionsPane.cpp
    ${HANDHELD}/src/client/gui/components/RolledSelectionListH.cpp
    ${HANDHELD}/src/client/gui/components/RolledSelectionListV.cpp
    ${HANDHELD}/src/client/gui/components/ScrolledSelectionList.cpp
    ${HANDHELD}/src/client/gui/components/ScrollingPane.cpp
    ${HANDHELD}/src/client/gui/components/Slider.cpp
    ${HANDHELD}/src/client/gui/components/SmallButton.cpp
    ${HANDHELD}/src/client/gui/Font.cpp
    ${HANDHELD}/src/client/gui/Gui.cpp
    ${HANDHELD}/src/client/gui/GuiComponent.cpp
    ${HANDHELD}/src/client/gui/Screen.cpp
    ${HANDHELD}/src/client/gui/screens/ScreenChooser.cpp
    ${HANDHELD}/src/client/gui/screens/ArmorScreen.cpp
    ${HANDHELD}/src/client/gui/screens/ChatScreen.cpp
    ${HANDHELD}/src/client/gui/screens/ConfirmScreen.cpp
    ${HANDHELD}/src/client/gui/screens/ChestScreen.cpp
    ${HANDHELD}/src/client/gui/screens/DeathScreen.cpp
    ${HANDHELD}/src/client/gui/screens/FurnaceScreen.cpp
    ${HANDHELD}/src/client/gui/screens/InBedScreen.cpp
    ${HANDHELD}/src/client/gui/screens/IngameBlockSelectionScreen.cpp
    ${HANDHELD}/src/client/gui/screens/JoinGameScreen.cpp
    ${HANDHELD}/src/client/gui/screens/OptionsScreen.cpp
    ${HANDHELD}/src/client/gui/screens/PauseScreen.cpp
    ${HANDHELD}/src/client/gui/screens/ProgressScreen.cpp
    ${HANDHELD}/src/client/gui/screens/RenameMPLevelScreen.cpp
    ${HANDHELD}/src/client/gui/screens/SelectWorldScreen.cpp
    ${HANDHELD}/src/client/gui/screens/StartMenuScreen.cpp
    ${HANDHELD}/src/client/gui/screens/TextEditScreen.cpp
    ${HANDHELD}/src/client/gui/screens/touch/TouchIngameBlockSelectionScreen.cpp
    ${HANDHELD}/src/client/gui/screens/touch/TouchJoinGameScreen.cpp
    ${HANDHELD}/src/client/gui/screens/touch/TouchSelectWorldScreen.cpp
    ${HANDHELD}/src/client/gui/screens/touch/TouchStartMenuScreen.cpp
    ${HANDHELD}/src/client/gui/screens/UploadPhotoScreen.cpp
    ${HANDHELD}/src/client/gui/screens/crafting/CraftingFilters.cpp
    ${HANDHELD}/src/client/gui/screens/crafting/PaneCraftingScreen.cpp
    ${HANDHELD}/src/client/gui/screens/crafting/StonecutterScreen.cpp
    ${HANDHELD}/src/client/gui/screens/crafting/WorkbenchScreen.cpp
    ${HANDHELD}/src/client/model/ChickenModel.cpp
    ${HANDHELD}/src/client/model/CowModel.cpp
    ${HANDHELD}/src/client/model/HumanoidModel.cpp
    ${HANDHELD}/src/client/model/PigModel.cpp
    ${HANDHELD}/src/client/model/SheepFurModel.cpp
    ${HANDHELD}/src/client/model/SheepModel.cpp
    ${HANDHELD}/src/client/model/QuadrupedModel.cpp
    ${HANDHELD}/src/client/model/geom/Cube.cpp
    ${HANDHELD}/src/client/model/geom/ModelPart.cpp
    ${HANDHELD}/src/client/model/geom/Polygon.cpp
    ${HANDHELD}/src/client/particle/Particle.cpp
    ${HANDHELD}/src/client/particle/ParticleEngine.cpp
    ${HANDHELD}/src/client/player/LocalPlayer.cpp
    ${HANDHELD}/src/client/player/RemotePlayer.cpp
    ${HANDHELD}/src/client/player/input/KeyboardInput.cpp
    ${HANDHELD}/src/client/player/input/touchscreen/TouchscreenInput.cpp
    ${HANDHELD}/src/client/renderer/Chunk.cpp
    ${HANDHELD}/src/client/renderer/EntityTileRenderer.cpp
    ${HANDHELD}/src/client/renderer/GameRenderer.cpp
    ${HANDHELD}/src/client/renderer/ItemInHandRenderer.cpp
    ${HANDHELD}/src/client/renderer/LevelRenderer.cpp
    ${HANDHELD}/src/client/renderer/RenderChunk.cpp
    ${HANDHELD}/src/client/renderer/RenderList.cpp
    ${HANDHELD}/src/client/renderer/Tesselator.cpp
    ${HANDHELD}/src/client/renderer/Textures.cpp
    ${HANDHELD}/src/client/renderer/TileRenderer.cpp
    ${HANDHELD}/src/client/renderer/gles.cpp
    ${HANDHELD}/src/client/renderer/culling/Frustum.cpp
    ${HANDHELD}/src/client/renderer/entity/ArrowRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/ChickenRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/EntityRenderDispatcher.cpp
    ${HANDHELD}/src/client/renderer/entity/EntityRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/FallingTileRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/HumanoidMobRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/ItemRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/ItemSpriteRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/MobRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/PaintingRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/PlayerRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/SheepRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/TntRenderer.cpp
    ${HANDHELD}/src/client/renderer/entity/TripodCameraRenderer.cpp
    ${HANDHELD}/src/client/renderer/ptexture/DynamicTexture.cpp
    ${HANDHELD}/src/client/renderer/tileentity/ChestRenderer.cpp
    ${HANDHELD}/src/client/renderer/tileentity/SignRenderer.cpp
    ${HANDHELD}/src/client/renderer/tileentity/TileEntityRenderDispatcher.cpp
    ${HANDHELD}/src/client/renderer/tileentity/TileEntityRenderer.cpp
    ${HANDHELD}/src/client/sound/Sound.cpp
    ${HANDHELD}/src/client/sound/SoundEngine.cpp
    ${HANDHELD}/src/locale/I18n.cpp
    ${HANDHELD}/src/nbt/Tag.cpp
    ${HANDHELD}/src/network/command/CommandServer.cpp
    ${HANDHELD}/src/network/ClientSideNetworkHandler.cpp
    ${HANDHELD}/src/network/NetEventCallback.cpp
    ${HANDHELD}/src/network/Packet.cpp
    ${HANDHELD}/src/network/RakNetInstance.cpp
    ${HANDHELD}/src/network/ServerSideNetworkHandler.cpp
    ${HANDHELD}/src/server/ServerLevel.cpp
    ${HANDHELD}/src/server/ServerPlayer.cpp
    ${HANDHELD}/src/util/DataIO.cpp
    ${HANDHELD}/src/util/Mth.cpp
    ${HANDHELD}/src/util/StringUtils.cpp
    ${HANDHELD}/src/util/PerfTimer.cpp
    ${HANDHELD}/src/util/PerfRenderer.cpp
    ${HANDHELD}/src/world/Direction.cpp
    ${HANDHELD}/src/world/entity/AgableMob.cpp
    ${HANDHELD}/src/world/entity/Entity.cpp
    ${HANDHELD}/src/world/entity/EntityFactory.cpp
    ${HANDHELD}/src/world/entity/FlyingMob.cpp
    ${HANDHELD}/src/world/entity/HangingEntity.cpp
    ${HANDHELD}/src/world/entity/Mob.cpp
    ${HANDHELD}/src/world/entity/MobCategory.cpp
    ${HANDHELD}/src/world/entity/Motive.cpp
    ${HANDHELD}/src/world/entity/Painting.cpp
    ${HANDHELD}/src/world/entity/PathfinderMob.cpp
    ${HANDHELD}/src/world/entity/SynchedEntityData.cpp
    ${HANDHELD}/src/world/entity/ai/control/MoveControl.cpp
    ${HANDHELD}/src/world/entity/animal/Animal.cpp
    ${HANDHELD}/src/world/entity/animal/Chicken.cpp
    ${HANDHELD}/src/world/entity/animal/Cow.cpp
    ${HANDHELD}/src/world/entity/animal/Pig.cpp
    ${HANDHELD}/src/world/entity/animal/Sheep.cpp
    ${HANDHELD}/src/world/entity/animal/WaterAnimal.cpp
    ${HANDHELD}/src/world/entity/item/FallingTile.cpp
    ${HANDHELD}/src/world/entity/item/ItemEntity.cpp
    ${HANDHELD}/src/world/entity/item/PrimedTnt.cpp
    ${HANDHELD}/src/world/entity/item/TripodCamera.cpp
    ${HANDHELD}/src/world/entity/monster/Creeper.cpp
    ${HANDHELD}/src/world/entity/monster/Monster.cpp
    ${HANDHELD}/src/world/entity/monster/PigZombie.cpp
    ${HANDHELD}/src/world/entity/monster/Skeleton.cpp
    ${HANDHELD}/src/world/entity/monster/Spider.cpp
    ${HANDHELD}/src/world/entity/monster/Zombie.cpp
    ${HANDHELD}/src/world/entity/projectile/Arrow.cpp
    ${HANDHELD}/src/world/entity/projectile/Throwable.cpp
    ${HANDHELD}/src/world/entity/player/Inventory.cpp
    ${HANDHELD}/src/world/entity/player/Player.cpp
    ${HANDHELD}/src/world/food/SimpleFoodData.cpp
    ${HANDHELD}/src/world/inventory/BaseContainerMenu.cpp
    ${HANDHELD}/src/world/inventory/ContainerMenu.cpp
    ${HANDHELD}/src/world/inventory/FillingContainer.cpp
    ${HANDHELD}/src/world/inventory/FurnaceMenu.cpp
    ${HANDHELD}/src/world/item/ArmorItem.cpp
    ${HANDHELD}/src/world/item/BedItem.cpp
    ${HANDHELD}/src/world/item/DyePowderItem.cpp
    ${HANDHELD}/src/world/item/Item.cpp
    ${HANDHELD}/src/world/item/ItemInstance.cpp
    ${HANDHELD}/src/world/item/HangingEntityItem.cpp
    ${HANDHELD}/src/world/item/HatchetItem.cpp
    ${HANDHELD}/src/world/item/HoeItem.cpp
    ${HANDHELD}/src/world/item/PickaxeItem.cpp
    ${HANDHELD}/src/world/item/ShovelItem.cpp
    ${HANDHELD}/src/world/item/crafting/ArmorRecipes.cpp
    ${HANDHELD}/src/world/item/crafting/Recipe.cpp
    ${HANDHELD}/src/world/item/crafting/Recipes.cpp
    ${HANDHELD}/src/world/item/crafting/FurnaceRecipes.cpp
    ${HANDHELD}/src/world/item/crafting/OreRecipes.cpp
    ${HANDHELD}/src/world/item/crafting/StructureRecipes.cpp
    ${HANDHELD}/src/world/item/crafting/ToolRecipes.cpp
    ${HANDHELD}/src/world/item/crafting/WeaponRecipes.cpp
    ${HANDHELD}/src/world/level/Explosion.cpp
    ${HANDHELD}/src/world/level/Level.cpp
    ${HANDHELD}/src/world/level/LightLayer.cpp
    ${HANDHELD}/src/world/level/LightUpdate.cpp
    ${HANDHELD}/src/world/level/MobSpawner.cpp
    ${HANDHELD}/src/world/level/Region.cpp
    ${HANDHELD}/src/world/level/TickNextTickData.cpp
    ${HANDHELD}/src/world/level/biome/Biome.cpp
    ${HANDHELD}/src/world/level/biome/BiomeSource.cpp
    ${HANDHELD}/src/world/level/chunk/LevelChunk.cpp
    ${HANDHELD}/src/world/level/dimension/Dimension.cpp
    ${HANDHELD}/src/world/level/levelgen/CanyonFeature.cpp
    ${HANDHELD}/src/world/level/levelgen/DungeonFeature.cpp
    ${HANDHELD}/src/world/level/levelgen/LargeCaveFeature.cpp
    ${HANDHELD}/src/world/level/levelgen/LargeFeature.cpp
    ${HANDHELD}/src/world/level/levelgen/RandomLevelSource.cpp
    ${HANDHELD}/src/world/level/levelgen/feature/Feature.cpp
    ${HANDHELD}/src/world/level/levelgen/synth/ImprovedNoise.cpp
    ${HANDHELD}/src/world/level/levelgen/synth/PerlinNoise.cpp
    ${HANDHELD}/src/world/level/levelgen/synth/Synth.cpp
    ${HANDHELD}/src/world/level/material/Material.cpp
    ${HANDHELD}/src/world/level/pathfinder/Path.cpp
    ${HANDHELD}/src/world/level/storage/ExternalFileLevelStorage.cpp
    ${HANDHELD}/src/world/level/storage/ExternalFileLevelStorageSource.cpp
    ${HANDHELD}/src/world/level/storage/FolderMethods.cpp
    ${HANDHELD}/src/world/level/storage/LevelData.cpp
    ${HANDHELD}/src/world/level/storage/LevelStorageSource.cpp
    ${HANDHELD}/src/world/level/storage/RegionFile.cpp
    ${HANDHELD}/src/world/level/tile/BedTile.cpp
    ${HANDHELD}/src/world/level/tile/ChestTile.cpp
    ${HANDHELD}/src/world/level/tile/CropTile.cpp
    ${HANDHELD}/src/world/level/tile/DoorTile.cpp
    ${HANDHELD}/src/world/level/tile/EntityTile.cpp
    ${HANDHELD}/src/world/level/tile/FurnaceTile.cpp
    ${HANDHELD}/src/world/level/tile/GrassTile.cpp
    ${HANDHELD}/src/world/level/tile/HeavyTile.cpp
    ${HANDHELD}/src/world/level/tile/LightGemTile.cpp
    ${HANDHELD}/src/world/level/tile/MelonTile.cpp
    ${HANDHELD}/src/world/level/tile/Mushroom.cpp
    ${HANDHELD}/src/world/level/tile/NetherReactor.cpp
    ${HANDHELD}/src/world/level/tile/NetherReactorPattern.cpp
    ${HANDHELD}/src/world/level/tile/StairTile.cpp
    ${HANDHELD}/src/world/level/tile/StemTile.cpp
    ${HANDHELD}/src/world/level/tile/StoneSlabTile.cpp
    ${HANDHELD}/src/world/level/tile/TallGrass.cpp
    ${HANDHELD}/src/world/level/tile/Tile.cpp
    ${HANDHELD}/src/world/level/tile/TrapDoorTile.cpp
    ${HANDHELD}/src/world/level/tile/entity/ChestTileEntity.cpp
    ${HANDHELD}/src/world/level/tile/entity/NetherReactorTileEntity.cpp
    ${HANDHELD}/src/world/level/tile/entity/SignTileEntity.cpp
    ${HANDHELD}/src/world/level/tile/entity/TileEntity.cpp
    ${HANDHELD}/src/world/level/tile/entity/FurnaceTileEntity.cpp
    ${HANDHELD}/src/world/phys/HitResult.cpp
)

# The OpenXR and GLES 3 layer is written against C++17 (designated-initialiser style
# braces); the engine stays on gnu++98. Separate targets keep the two dialects apart.
add_library(vrlayer STATIC
    ${HANDHELD}/src/platform/vr/vr_state.cpp
    ${HANDHELD}/src/platform/vr/vr_render.cpp
    ${HANDHELD}/src/platform/vr/xr_session.cpp
)
set_target_properties(vrlayer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
target_compile_options(vrlayer PRIVATE -Wno-psabi)
target_include_directories(vrlayer PRIVATE ${HANDHELD}/src)
target_link_libraries(vrlayer PRIVATE openxr_loader)

# Khronos OpenXR loader and headers. Pinned to the commit this port was built and
# checked against; the loader is built for the same NDK as the game.
include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)
FetchContent_Declare(openxr_sdk
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG f2448a8797c85814aa892efc1ab8707900fbcc78
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(openxr_sdk)
# Put libopenxr_loader.so next to the game library so the APK packages it.
set_target_properties(openxr_loader PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(RAKNET_SOURCES
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/BitStream.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CCRakNetSlidingWindow.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CCRakNetUDT.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CheckSum.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CloudClient.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CloudCommon.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CloudServer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/CommandParserInterface.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ConnectionGraph2.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ConsoleServer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DS_BytePool.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DS_ByteQueue.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DS_HuffmanEncodingTree.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DS_Table.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DataCompressor.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DirectoryDeltaTransfer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/DynDNS.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/EmailSender.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/EncodeClassName.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/EpochTimeToString.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/FileList.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/FileListTransfer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/FileOperations.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/FormatString.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/FullyConnectedMesh2.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/GetTime.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Getche.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Gets.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/GridSectorizer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/HTTPConnection.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/IncrementalReadInterface.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Itoa.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/LinuxStrings.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/LocklessTypes.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/LogCommandParser.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/MessageFilter.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NatPunchthroughClient.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NatPunchthroughServer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NatTypeDetectionClient.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NatTypeDetectionCommon.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NatTypeDetectionServer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NetworkIDManager.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/NetworkIDObject.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PacketConsoleLogger.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PacketFileLogger.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PacketLogger.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PacketOutputWindowLogger.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PacketizedTCP.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/PluginInterface2.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RPC4Plugin.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Rackspace.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakMemoryOverride.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakNetCommandParser.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakNetSocket.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakNetStatistics.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakNetTransport2.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakNetTypes.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakPeer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakSleep.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakString.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakThread.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/RakWString.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Rand.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ReadyEvent.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ReliabilityLayer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ReplicaManager3.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/Router2.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SHA1.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SecureHandshake.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SendToThread.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SignaledEvent.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SimpleMutex.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SocketLayer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/StringCompressor.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/StringTable.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/SuperFastHash.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/TCPInterface.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/TableSerializer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/TeamBalancer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/TelnetTransport.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/ThreadsafePacketLogger.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/TwoWayAuthentication.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/UDPForwarder.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/UDPProxyClient.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/UDPProxyCoordinator.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/UDPProxyServer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/VariableDeltaSerializer.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/VariableListDeltaTracker.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/VariadicSQLParser.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/VitaIncludes.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/WSAStartupSingleton.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/_FindFirst.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/gettimeofday.cpp
    ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources/rdlmalloc.cpp
)

add_library(raknet STATIC ${RAKNET_SOURCES})
target_include_directories(raknet PRIVATE ${HANDHELD}/project/lib_projects/raknet/jni/RaknetSources)

# NDK activity glue (NativeActivity event loop helpers)
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(minecraftpe SHARED ${GAME_SOURCES})
target_include_directories(minecraftpe PRIVATE ${HANDHELD}/src ${HANDHELD}/src/raknet)
target_compile_definitions(minecraftpe PRIVATE ANDROID MCPE_VR=1)
target_compile_options(minecraftpe PRIVATE -Wno-psabi -fpermissive -Wno-deprecated-declarations -Wno-invalid-offsetof -Wno-error=format-security)
target_link_libraries(minecraftpe PRIVATE vrlayer raknet native_app_glue openxr_loader log android EGL GLESv3 OpenSLES)
