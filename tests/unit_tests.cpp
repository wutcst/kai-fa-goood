#include "LevelCatalog.hpp"
#include "LevelMechanics.hpp"
#include "Map.hpp"
#include "Physics.hpp"
#include "Protocol.hpp"
#include "TmxUtil.hpp"

#include <gtest/gtest.h>

namespace {

TEST(MapTest, LoadFromTextParsesDimensionsAndTiles) {
    fireice::GameMap map;
    const std::string text =
        "###\n"
        "#.#\n"
        "###\n";

    ASSERT_TRUE(map.loadFromText(text));
    EXPECT_EQ(map.width(), 3);
    EXPECT_EQ(map.height(), 3);
    EXPECT_TRUE(map.isSolid(map.tileAt(0, 0)));
    EXPECT_FALSE(map.isSolid(map.tileAt(1, 1)));
}

TEST(MapTest, LoadFromTextCollectsSpawnPoints) {
    fireice::GameMap map;
    const std::string text =
        "f.w\n"
        ".p.\n";

    ASSERT_TRUE(map.loadFromText(text));
    ASSERT_EQ(map.spawns().size(), 3u);
    EXPECT_EQ(map.spawns()[0].role, fireice::PlayerRole::Fire);
    EXPECT_EQ(map.spawns()[1].role, fireice::PlayerRole::Water);
    EXPECT_EQ(map.spawns()[2].role, fireice::PlayerRole::Poison);
}

TEST(MapTest, HazardImmunityFollowsRole) {
    fireice::GameMap map;
    EXPECT_TRUE(map.isHazardFor(fireice::TileType::Lava, fireice::PlayerRole::Water));
    EXPECT_FALSE(map.isHazardFor(fireice::TileType::Lava, fireice::PlayerRole::Fire));
    EXPECT_TRUE(map.isHazardFor(fireice::TileType::Acid, fireice::PlayerRole::Fire));
    EXPECT_FALSE(map.isHazardFor(fireice::TileType::Acid, fireice::PlayerRole::Poison));
}

TEST(ProtocolTest, PackAndUnpackInputPacket) {
    fireice::InputPacket original{};
    original.slot = 2;
    original.tick = 42;
    original.flags = 0b1010;

    std::array<char, fireice::PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    ASSERT_TRUE(fireice::packPacket(original, buffer, size));

    fireice::InputPacket decoded{};
    ASSERT_TRUE(fireice::unpackPacket(buffer.data(), size, decoded));
    EXPECT_EQ(decoded.slot, 2);
    EXPECT_EQ(decoded.tick, 42u);
    EXPECT_EQ(decoded.flags, 0b1010);
}

TEST(ProtocolTest, RejectsTruncatedPacket) {
    fireice::InputPacket packet{};
    std::array<char, fireice::PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    ASSERT_TRUE(fireice::packPacket(packet, buffer, size));

    fireice::InputPacket decoded{};
    EXPECT_FALSE(fireice::unpackPacket(buffer.data(), size - 1, decoded));
}

TEST(LevelCatalogTest, AllRegisteredLevelsSupportThreePlayers) {
    const fireice::LevelCatalog& catalog = fireice::LevelCatalog::instance();
    EXPECT_GE(catalog.count(), 8u);
    EXPECT_EQ(catalog.countForPlayerCount(3), catalog.count());
}

TEST(LevelCatalogTest, FilteredIndexRoundTrip) {
    const fireice::LevelCatalog& catalog = fireice::LevelCatalog::instance();
    const uint8_t playerCount = 2;
    const uint8_t filteredCount = catalog.countForPlayerCount(playerCount);
    ASSERT_GT(filteredCount, 0u);

    for (uint8_t filtered = 0; filtered < filteredCount; ++filtered) {
        const uint8_t global = catalog.filteredIndexToGlobalIndex(filtered, playerCount);
        EXPECT_EQ(catalog.globalIndexToFilteredIndex(global, playerCount), filtered);
    }
}

TEST(TmxUtilTest, FindImageLayerInsideImageLayerTag) {
    const std::string xml =
        R"(<imagelayer id="2" name="Background" repeatx="1" repeaty="1">
  <image source="level03/bule.png" width="64" height="64"/>
 </imagelayer>)";

    const std::vector<fireice::tmx::ImageLayerData> layers = fireice::tmx::findAllImageLayers(xml);
    ASSERT_EQ(layers.size(), 1u);
    EXPECT_EQ(layers.front().imageSource, "level03/bule.png");
    EXPECT_TRUE(layers.front().repeatX);
    EXPECT_TRUE(layers.front().repeatY);
}

TEST(PowerUpTest, CollectsMagnetAndSpeedBoostTogether) {
    fireice::LevelRuntime runtime;
    fireice::PlayerState player{};
    player.alive = true;
    player.x = 100.0f;
    player.y = 200.0f;

    runtime.magnetDrops[0].active = true;
    runtime.magnetDrops[0].kind = static_cast<uint8_t>(fireice::PowerUpKind::Magnet);
    runtime.magnetDrops[0].x = player.x + fireice::PLAYER_WIDTH * 0.5f;
    runtime.magnetDrops[0].y = player.y;
    runtime.magnetDrops[0].falling = false;

    runtime.magnetDrops[1].active = true;
    runtime.magnetDrops[1].kind = static_cast<uint8_t>(fireice::PowerUpKind::SpeedBoost);
    runtime.magnetDrops[1].x = player.x + fireice::PLAYER_WIDTH * 0.5f;
    runtime.magnetDrops[1].y = player.y;
    runtime.magnetDrops[1].falling = false;

    fireice::collectMagnetDrops(player, runtime);

    EXPECT_GT(player.magnetTimer, 0.0f);
    EXPECT_GT(player.speedBoostTimer, 0.0f);
    EXPECT_FALSE(runtime.magnetDrops[0].active);
    EXPECT_FALSE(runtime.magnetDrops[1].active);
}

}  // namespace
