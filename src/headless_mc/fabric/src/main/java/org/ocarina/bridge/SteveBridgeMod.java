package org.ocarina.bridge;

import com.mojang.authlib.GameProfile;
import com.mojang.serialization.JsonOps;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.minecraft.component.DataComponentTypes;
import net.minecraft.entity.EquipmentSlot;
import net.minecraft.entity.ItemEntity;
import net.minecraft.item.ItemStack;
import net.minecraft.item.Items;
import net.minecraft.block.Blocks;
import net.minecraft.network.ClientConnection;
import net.minecraft.network.NetworkSide;
import net.minecraft.network.packet.Packet;
import net.minecraft.network.packet.c2s.common.SyncedClientOptions;
import net.minecraft.network.packet.c2s.play.PlayerActionC2SPacket;
import net.minecraft.registry.Registries;
import net.minecraft.registry.RegistryOps;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ConnectedClientData;
import net.minecraft.server.network.ServerPlayNetworkHandler;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.util.Hand;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Direction;
import net.minecraft.util.math.Vec3d;
import net.minecraft.world.GameMode;
import net.minecraft.util.WorldSavePath;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayDeque;
import java.util.UUID;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public final class SteveBridgeMod implements ModInitializer {
    private static final Logger LOG = LoggerFactory.getLogger("SteveBridge");
    private ServerPlayerEntity player;
    private ServerWorld world;
    private boolean connected;
    private long tick, sequence, inputTime, blockEvent;
    private int buttons, pressed, lastButtons, slot, locked, scene = -1, stage = -1, hand, flags;
    private float x, y, z, yaw, pitch, tx, ty, tz, originX, originY, originZ;
    private float damage, miningProgress;
    private BlockPos mining;
    private int miningBlockId;
    private Path playerFile;
    private final ArrayDeque<Integer> sounds = new ArrayDeque<>();

    @Override public void onInitialize() {
        ServerLifecycleEvents.SERVER_STARTED.register(this::start);
        ServerLifecycleEvents.SERVER_STOPPING.register(server -> { savePlayer(server); NativeBridge.close(); });
        ServerTickEvents.END_SERVER_TICK.register(this::tick);
    }
    private void start(MinecraftServer server) {
        world = server.getOverworld();
        GameProfile profile = new GameProfile(UUID.fromString("22eba9f5-d003-4fd3-a9bf-028a92fca065"), "SteveBridge");
        player = new ServerPlayerEntity(server, world, profile, SyncedClientOptions.createDefault());
        player.networkHandler = new ServerPlayNetworkHandler(server,
                new ClientConnection(NetworkSide.SERVERBOUND), player, ConnectedClientData.createDefault(profile, false)) {
            @Override public void sendPacket(Packet<?> packet) {} // No client, no packet queue.
        };
        player.changeGameMode(GameMode.SURVIVAL);
        player.setNoGravity(true);
        player.refreshPositionAndAngles(0.5, 65, 0.5, 0, 0);
        player.getInventory().setStack(0, new ItemStack(Items.IRON_PICKAXE));
        player.getInventory().setStack(1, new ItemStack(Items.IRON_SWORD));
        player.getInventory().setStack(2, new ItemStack(Items.COOKED_BEEF, 8));
        player.getInventory().setStack(3, new ItemStack(Items.BOW));
        player.getInventory().setStack(8, new ItemStack(Items.ARROW, 64));
        player.equipStack(EquipmentSlot.OFFHAND, new ItemStack(Items.SHIELD));
        player.getHungerManager().setFoodLevel(12);
        playerFile = server.getSavePath(WorldSavePath.ROOT).resolve("steve-player.json");
        loadPlayer(server);
        // Bound the generated world to a single small, persistent arena.
        for (int bx = -8; bx <= 8; bx++) for (int bz = -8; bz <= 8; bz++)
            world.setBlockState(new BlockPos(bx, 64, bz), Blocks.BEDROCK.getDefaultState());
        world.setBlockState(new BlockPos(0, 65, 2), Blocks.STONE.getDefaultState());
        LOG.info("STEVE_BRIDGE_READY Minecraft=1.21.11 nativeProtocol=2");
    }
    private void tick(MinecraftServer server) {
        if (player == null) return;
        tick++;
        if (!connected) connected = NativeBridge.open(System.getProperty("steve.session", "default"));
        if (!connected) return;
        if (!NativeBridge.alive()) { NativeBridge.close(); connected = false; return; }
        pressed = 0;
        damage = 0;
        for (ByteBuffer packet; (packet = NativeBridge.peek()) != null;) {
            try { read(packet.order(ByteOrder.nativeOrder())); }
            finally { NativeBridge.consume(); }
        }
        long now = NativeBridge.now();
        if (now < inputTime || now - inputTime > 250_000_000L) { buttons = 0; pressed = 0; }
        else pressed |= buttons & ~lastButtons;
        lastButtons = buttons;
        player.getInventory().setSelectedSlot(Math.max(0, Math.min(8, slot)));
        // SoH owns traversal; Minecraft owns items, health and interactions.
        double px = Math.max(-6, Math.min(6, x - originX));
        double py = Math.max(0, Math.min(4, y - originY));
        double pz = Math.max(-6, Math.min(6, z - originZ));
        player.refreshPositionAndAngles(px + 0.5, 65 + py, pz + 0.5,
                -(float)Math.toDegrees(yaw), (float)Math.toDegrees(pitch));
        if (Float.isFinite(damage) && damage > 0) player.damage(world, world.getDamageSources().generic(), Math.min(damage, 20));
        if ((flags & 1) != 0) { player.setHealth(20); player.getHungerManager().setFoodLevel(12); flags=0; }
        hand = 0;
        boolean holdingUse = (buttons & 1) != 0 || (pressed & 1) != 0;
        if ((buttons & 8) != 0) {
            abortMining();
            if (!player.isUsingItem()) player.setCurrentHand(Hand.OFF_HAND);
            hand = 3;
        } else if ((buttons & 2) != 0) {
            if (player.isUsingItem()) player.stopUsingItem();
            ItemStack held = player.getMainHandStack();
            if (held.isOf(Items.IRON_PICKAXE)) { hand = 1; mine(); }
            else {
                hand = 2; abortMining();
                if ((pressed & 2) != 0) {
                    var targets = world.getOtherEntities(player, player.getBoundingBox().expand(3), e -> e.isAlive() && !(e instanceof ItemEntity));
                    if (!targets.isEmpty()) player.attack(targets.getFirst());
                    player.swingHand(Hand.MAIN_HAND); sound(1);
                }
            }
        } else {
            abortMining();
            ItemStack held = player.getMainHandStack();
            if (holdingUse && !player.isUsingItem()) {
                player.interactionManager.interactItem(player, world, held, Hand.MAIN_HAND);
            } else if (!holdingUse && player.isUsingItem()) {
                if (held.isOf(Items.BOW)) { player.stopUsingItem(); sound(4); }
                else player.clearActiveItem();
            }
            if (player.isUsingItem()) hand = held.isOf(Items.BOW) ? 5 : 4;
        }
        int oldFoodCount = player.getInventory().getStack(2).getCount();
        // Fake players have no world/network connection to drive their personal tick.
        player.tick();
        player.playerTick();
        if (player.getInventory().getStack(2).getCount() < oldFoodCount) sound(3);
        player.setVelocity(Vec3d.ZERO);
        for (ItemEntity drop : world.getEntitiesByClass(ItemEntity.class, player.getBoundingBox().expand(5), e -> true)) {
            ItemStack stack = drop.getStack();
            player.getInventory().insertStack(stack);
            if (stack.isEmpty()) drop.discard();
        }
        publish();
        if (tick % 100 == 0) savePlayer(server);
    }
    private void loadPlayer(MinecraftServer server) {
        if (!Files.exists(playerFile)) return;
        try {
            JsonObject data = JsonParser.parseString(Files.readString(playerFile)).getAsJsonObject();
            var ops = RegistryOps.of(JsonOps.INSTANCE, server.getRegistryManager());
            JsonArray items = data.getAsJsonArray("inventory");
            for (int i=0;i<Math.min(items.size(),36);i++)
                player.getInventory().setStack(i, ItemStack.OPTIONAL_CODEC.parse(ops,items.get(i)).getOrThrow());
            player.equipStack(EquipmentSlot.OFFHAND, ItemStack.OPTIONAL_CODEC.parse(ops,data.get("offhand")).getOrThrow());
            player.setHealth(Math.max(0,Math.min(20,data.get("health").getAsFloat())));
            player.getHungerManager().setFoodLevel(Math.max(0,Math.min(20,data.get("food").getAsInt())));
        } catch (Exception e) { LOG.error("Cannot load persisted bridge player; using initial equipment",e); }
    }
    private void savePlayer(MinecraftServer server) {
        if (player == null || playerFile == null) return;
        try {
            var ops = RegistryOps.of(JsonOps.INSTANCE, server.getRegistryManager());
            JsonObject data = new JsonObject();
            JsonArray items = new JsonArray();
            for (int i=0;i<36;i++) items.add(ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getInventory().getStack(i)).getOrThrow());
            data.add("inventory",items);
            data.add("offhand",ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getOffHandStack()).getOrThrow());
            data.addProperty("health",player.getHealth());
            data.addProperty("food",player.getHungerManager().getFoodLevel());
            Path temporary = playerFile.resolveSibling("steve-player.json.tmp");
            Files.writeString(temporary,new GsonBuilder().create().toJson(data));
            Files.move(temporary,playerFile,StandardCopyOption.REPLACE_EXISTING);
        } catch (Exception e) { LOG.error("Cannot save bridge player",e); }
    }
    private void read(ByteBuffer p) {
        sequence = p.getLong(0); inputTime = p.getLong(8);
        x = p.getFloat(16); y = p.getFloat(20); z = p.getFloat(24);
        yaw = p.getFloat(28); pitch = p.getFloat(32);
        buttons = p.getInt(36); locked = p.getInt(40); slot = p.getInt(44);
        tx = p.getFloat(48); ty = p.getFloat(52); tz = p.getFloat(56);
        pressed |= p.getInt(60); damage += p.getFloat(64);
        int newScene = p.getInt(68);
        flags |= p.getInt(72);
        if (scene != newScene) {
            scene = newScene; originX = x; originY = y; originZ = z; abortMining();
            world.setBlockState(new BlockPos(0,65,2),Blocks.STONE.getDefaultState());
        }
        if (!Float.isFinite(x+y+z+yaw+pitch+tx+ty+tz)) { buttons = 0; pressed = 0; }
    }
    private void mine() {
        BlockPos target;
        if (locked != 0) target = BlockPos.ofFloored(tx-originX+0.5, ty-originY+65, tz-originZ+0.5);
        else target = new BlockPos(0, 65, 2); // Arena block when Zelda has no actor target.
        if (player.squaredDistanceTo(target.toCenterPos()) > 25 || world.getBlockState(target).isAir()) { abortMining(); return; }
        if (!target.equals(mining)) {
            abortMining(); mining = target;
            miningBlockId = Registries.BLOCK.getRawId(world.getBlockState(target).getBlock());
            player.interactionManager.processBlockBreakingAction(target, PlayerActionC2SPacket.Action.START_DESTROY_BLOCK,
                    Direction.UP, world.getTopYInclusive(), (int)sequence);
        }
        if (world.getBlockState(target).isAir()) { completed(); return; }
        miningProgress += world.getBlockState(target).calcBlockBreakingDelta(player, world, target);
        stage = Math.max(0, Math.min(9, (int)(miningProgress * 10)));
        player.swingHand(Hand.MAIN_HAND);
        if (miningProgress >= 1) {
            player.interactionManager.processBlockBreakingAction(target, PlayerActionC2SPacket.Action.STOP_DESTROY_BLOCK,
                    Direction.UP, world.getTopYInclusive(), (int)sequence);
            if (world.getBlockState(target).isAir()) completed();
        }
    }
    private void completed() { blockEvent++; sound(2); mining = null; miningProgress = 0; stage = -1; }
    private void abortMining() {
        if (mining != null) player.interactionManager.processBlockBreakingAction(mining,
                PlayerActionC2SPacket.Action.ABORT_DESTROY_BLOCK, Direction.UP, world.getTopYInclusive(), (int)sequence);
        mining = null; miningProgress = 0; stage = -1;
    }
    private void sound(int value) { if (sounds.size() < 64) sounds.addLast(value); }
    private void item(ByteBuffer p, int offset, ItemStack stack) {
        p.putInt(offset, stack.isEmpty() ? 0 : Registries.ITEM.getRawId(stack.getItem()));
        p.putInt(offset + 4, stack.getDamage());
        p.putInt(offset + 8, stack.getCount());
        p.putInt(offset + 12, stack.isDamageable() ? stack.getMaxDamage() - stack.getDamage() : 0);
    }
    private void publish() {
        ByteBuffer p = NativeBridge.reserve();
        if (p == null) return;
        p.order(ByteOrder.nativeOrder());
        p.putLong(0,tick); p.putLong(16,sequence);
        item(p,24,player.getMainHandStack());
        p.putInt(40,hand); p.putFloat(44,player.getHealth()); p.putFloat(48,player.handSwingProgress);
        p.putInt(52,stage); p.putInt(56,player.getInventory().getSelectedSlot());
        int count = Math.min(8, sounds.size()); p.putInt(60,count);
        for(int i=0;i<count;i++) p.putInt(64+i*4,sounds.removeFirst());
        for(int i=0;i<9;i++) item(p,96+i*16,player.getInventory().getStack(i));
        p.putLong(240,blockEvent); p.putInt(248,miningBlockId);
        ItemStack held=player.getMainHandStack();
        int kind=held.isOf(Items.IRON_PICKAXE)?1:held.isOf(Items.IRON_SWORD)?2:held.isOf(Items.COOKED_BEEF)?3:held.isOf(Items.BOW)?4:held.isEmpty()?0:5;
        p.putInt(252,1 | (kind<<8));
        NativeBridge.publish();
    }
}
