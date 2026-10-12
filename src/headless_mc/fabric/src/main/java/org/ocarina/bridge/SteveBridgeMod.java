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
import net.minecraft.entity.EntityType;
import net.minecraft.entity.TntEntity;
import net.minecraft.entity.mob.ZombieEntity;
import net.minecraft.entity.attribute.EntityAttributes;
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
import net.minecraft.registry.RegistryKeys;
import net.minecraft.enchantment.Enchantments;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ConnectedClientData;
import net.minecraft.server.network.ServerPlayNetworkHandler;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.util.Hand;
import net.minecraft.util.Unit;
import net.minecraft.component.type.FireworksComponent;
import java.util.List;
import net.minecraft.util.math.BlockPos;
import net.minecraft.util.math.Direction;
import net.minecraft.util.math.Vec3d;
import net.minecraft.util.hit.BlockHitResult;
import net.minecraft.util.math.Box;
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
    private long commandAck,attackEvent,tntEvent;
    private int commandResult,targetToken,targetEnemy,attackTarget;
    private float targetHealth,attackDamage;
    private ZombieEntity combatProxy;
    private BlockPos placedTnt;
    private TntEntity primedTnt;
    private int tntFuse=-2;
    private boolean flying;
    private Vec3d flightVelocity=Vec3d.ZERO;
    private int rocketBoostTicks;
    private static class Cell { BlockPos shadow; Vec3d position=Vec3d.ZERO; int kind,flags; long token; boolean active; }
    private final Cell[] cells=new Cell[128];
    private Cell miningCell;
    private Vec3d terrainTarget;
    private long cellToken;
    private static class Charge { BlockPos position; Vec3d nativePosition=Vec3d.ZERO; TntEntity entity; int fuse=-2,unresolvedTicks; long token; }
    private final Charge[] charges=new Charge[8];
    public SteveBridgeMod() {for(int i=0;i<charges.length;i++)charges[i]=new Charge();for(int i=0;i<cells.length;i++)cells[i]=new Cell();}
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
        player = new ServerPlayerEntity(server, world, profile, SyncedClientOptions.createDefault()) {
            @Override public ItemStack getMainHandStack() {return slot==9?getInventory().getStack(9):super.getMainHandStack();}
            @Override public void setStackInHand(Hand hand,ItemStack stack) {
                if(hand==Hand.MAIN_HAND && slot==9)getInventory().setStack(9,stack);else super.setStackInHand(hand,stack);
            }
        };
        player.networkHandler = new ServerPlayNetworkHandler(server,
                new ClientConnection(NetworkSide.SERVERBOUND), player, ConnectedClientData.createDefault(profile, false)) {
            @Override public void sendPacket(Packet<?> packet) {} // No client, no packet queue.
        };
        player.changeGameMode(GameMode.SURVIVAL);
        player.setNoGravity(true);
        player.refreshPositionAndAngles(0.5, 65, 0.5, 0, 0);
        starterLoadout();
        player.equipStack(EquipmentSlot.OFFHAND, new ItemStack(Items.SHIELD));
        player.getHungerManager().setFoodLevel(12);
        playerFile = server.getSavePath(WorldSavePath.ROOT).resolve("steve-player.json");
        loadPlayer(server);
        equipDefaultArmor(server);
        // Bound the generated world to a single small, persistent arena.
        for (int bx = -8; bx <= 8; bx++) for (int bz = -8; bz <= 8; bz++)
            world.setBlockState(new BlockPos(bx, 64, bz), Blocks.BEDROCK.getDefaultState());
        world.setBlockState(new BlockPos(0, 65, 2), Blocks.STONE.getDefaultState());
        world.setChunkForced(0,0,true);
        combatProxy=new ZombieEntity(EntityType.ZOMBIE,world);
        combatProxy.setAiDisabled(true); combatProxy.setSilent(true);
        combatProxy.getAttributeInstance(EntityAttributes.MAX_HEALTH).setBaseValue(1024);
        LOG.info("STEVE_BRIDGE_READY Minecraft=1.21.11 nativeProtocol=4");
    }
    private void starterLoadout() {
        var items=new net.minecraft.item.Item[]{Items.DIAMOND_AXE,Items.DIAMOND_PICKAXE,Items.GOLDEN_APPLE,Items.FLINT_AND_STEEL,
            Items.COBBLESTONE,Items.OBSIDIAN,Items.ELYTRA,Items.FIREWORK_ROCKET,Items.TOTEM_OF_UNDYING,Items.TNT};
        int[] counts={1,1,64,1,64,32,1,64,1,64};
        for(int i=0;i<items.length;i++)player.getInventory().setStack(i,new ItemStack(items[i],counts[i]));
        player.getInventory().getStack(6).set(DataComponentTypes.UNBREAKABLE,Unit.INSTANCE);
        player.getInventory().getStack(7).set(DataComponentTypes.FIREWORKS,new FireworksComponent(1,List.of()));
    }
    private boolean infinite(ItemStack stack) {return stack.isOf(Items.GOLDEN_APPLE)||stack.isOf(Items.COBBLESTONE)||stack.isOf(Items.FIREWORK_ROCKET)||stack.isOf(Items.TNT);}
    private void restock() {
        for(int i=0;i<36;i++){var stack=player.getInventory().getStack(i);if(infinite(stack))stack.setCount(64);}
        if(!player.getOffHandStack().isOf(Items.SHIELD))player.equipStack(EquipmentSlot.OFFHAND,new ItemStack(Items.SHIELD));
    }
    private void tick(MinecraftServer server) {
        if (player == null) return;
        tick++;
        if (!connected) connected = NativeBridge.open(System.getProperty("steve.session", "default"));
        if (!connected) return;
        if (!NativeBridge.alive()) {
            player.clearActiveItem(); abortMining(); lastButtons=0;
            NativeBridge.close(); connected = false; return;
        }
        pressed = 0;
        damage = 0;
        for (ByteBuffer packet; (packet = NativeBridge.peek()) != null;) {
            try { read(packet.order(ByteOrder.nativeOrder())); }
            finally { NativeBridge.consume(); }
        }
        long now = NativeBridge.now();
        if (now < inputTime || now - inputTime > 250_000_000L) {
            buttons = 0; pressed = 0;
            // A lost frontend cancels a draw; it is not an intentional bow release.
            player.clearActiveItem();
            flying=false;
        }
        else pressed |= buttons & ~lastButtons;
        lastButtons = buttons;
        player.getInventory().setSelectedSlot(slot==9?0:Math.max(0, Math.min(8, slot)));
        // SoH owns traversal; Minecraft owns items, health and interactions.
        double px = Math.max(-6, Math.min(6, x - originX));
        double py = Math.max(0, Math.min(4, y - originY));
        double pz = Math.max(-6, Math.min(6, z - originZ));
        player.refreshPositionAndAngles(px + 0.5, 65 + py, pz + 0.5,
                -(float)Math.toDegrees(yaw), (float)Math.toDegrees(pitch));
        player.setOnGround((flags & 2) != 0);
        if (Float.isFinite(damage) && damage > 0) player.damage(world, world.getDamageSources().generic(), Math.min(damage, 20));
        if ((flags & 1) != 0) { player.setHealth(20); player.getHungerManager().setFoodLevel(12); flags &= ~1; }
        hand = 0;
        boolean holdingUse = (buttons & 1) != 0 || (pressed & 1) != 0;
        if ((buttons & 8) != 0) {
            abortMining();
            if (!player.isUsingItem()) player.setCurrentHand(Hand.OFF_HAND);
            hand = 3;
        } else if ((buttons & 2) != 0) {
            if (player.isUsingItem()) player.stopUsingItem();
            ItemStack held = player.getMainHandStack();
            if ((held.isOf(Items.DIAMOND_PICKAXE) || held.isOf(Items.DIAMOND_AXE) && (flags&4)!=0) && targetEnemy==0) { hand = 1; mine(); }
            else {
                hand = 2; abortMining();
                if ((pressed & 2) != 0) {
                    attackTarget=targetToken; attackDamage=0;
                    Vec3d targetPosition=new Vec3d(player.getX()+tx-x,player.getY()+ty-y,player.getZ()+tz-z);
                    if(targetEnemy!=0 && targetToken!=0 && targetHealth>0 && player.squaredDistanceTo(targetPosition)<9) {
                        combatProxy.setPosition(targetPosition);
                        combatProxy.setHealth(Math.min(1024,targetHealth*2));
                        combatProxy.timeUntilRegen=0;
                        float before=combatProxy.getHealth(); player.attack(combatProxy);
                        attackDamage=Math.max(0,before-combatProxy.getHealth()); attackEvent++;
                    } else {
                        var targets = world.getOtherEntities(player, player.getBoundingBox().expand(3), e -> e.isAlive() && !(e instanceof ItemEntity));
                        if (!targets.isEmpty()) player.attack(targets.getFirst());
                    }
                    player.swingHand(Hand.MAIN_HAND); sound(1);
                }
            }
        } else {
            abortMining();
            ItemStack held = player.getMainHandStack();
            if (holdingUse && !player.isUsingItem()) {
                if((pressed&1)!=0 && (held.isOf(Items.COBBLESTONE)||held.isOf(Items.OBSIDIAN))) placeCell(held);
                else if((pressed&1)!=0 && (held.isOf(Items.TNT)||held.isOf(Items.FLINT_AND_STEEL))) useBlock(held);
                else if((pressed&1)!=0 && held.isOf(Items.FIREWORK_ROCKET) && flying) {
                    player.interactionManager.interactItem(player,world,held,Hand.MAIN_HAND);rocketBoostTicks=30;
                }
                else if(!held.isOf(Items.TNT)&&!held.isOf(Items.FLINT_AND_STEEL)) player.interactionManager.interactItem(player, world, held, Hand.MAIN_HAND);
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
        restock();
        if (player.getInventory().getStack(2).getCount() < oldFoodCount) sound(3);
        if(flying && !player.getEquippedStack(EquipmentSlot.CHEST).isOf(Items.ELYTRA)) flying=false;
        if(flying && (flags&2)!=0) flying=false;
        if(flying) {
            player.startGliding();
            if(rocketBoostTicks>0){flightVelocity=player.getVelocity();rocketBoostTicks--;}
            player.setVelocity(flightVelocity);
            player.travel(Vec3d.ZERO); flightVelocity=player.getVelocity();
        } else { player.stopGliding(); flightVelocity=Vec3d.ZERO; player.setVelocity(Vec3d.ZERO); }
        updateTnt();
        for(Cell cell:cells)if(cell.active && cell.flags==0 && world.getBlockState(cell.shadow).isAir())cell.active=false;
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
            if(data.has("chest")) player.equipStack(EquipmentSlot.CHEST,ItemStack.OPTIONAL_CODEC.parse(ops,data.get("chest")).getOrThrow());
            for(var slot:new EquipmentSlot[]{EquipmentSlot.HEAD,EquipmentSlot.LEGS,EquipmentSlot.FEET})
                if(data.has(slot.getName())) player.equipStack(slot,ItemStack.OPTIONAL_CODEC.parse(ops,data.get(slot.getName())).getOrThrow());
            player.setHealth(Math.max(0,Math.min(20,data.get("health").getAsFloat())));
            player.getHungerManager().setFoodLevel(Math.max(0,Math.min(20,data.get("food").getAsInt())));
            if(!data.has("equipmentVersion")) {
                grantMissing(new ItemStack(Items.NETHERITE_AXE),4);
                grantMissing(new ItemStack(Items.TNT,16),5);
                grantMissing(new ItemStack(Items.FLINT_AND_STEEL),6);
                grantMissing(new ItemStack(Items.ELYTRA),7);
            }
            if(!data.has("loadoutVersion") || data.get("loadoutVersion").getAsInt()<2) starterLoadout();
        } catch (Exception e) { LOG.error("Cannot load persisted bridge player; using initial equipment",e); }
    }
    private void savePlayer(MinecraftServer server) {
        if (player == null || playerFile == null) return;
        try {
            var ops = RegistryOps.of(JsonOps.INSTANCE, server.getRegistryManager());
            JsonObject data = new JsonObject();
            data.addProperty("equipmentVersion",1);
            data.addProperty("loadoutVersion",2);
            JsonArray items = new JsonArray();
            for (int i=0;i<36;i++) items.add(ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getInventory().getStack(i)).getOrThrow());
            data.add("inventory",items);
            data.add("offhand",ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getOffHandStack()).getOrThrow());
            data.add("chest",ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getEquippedStack(EquipmentSlot.CHEST)).getOrThrow());
            for(var slot:new EquipmentSlot[]{EquipmentSlot.HEAD,EquipmentSlot.LEGS,EquipmentSlot.FEET})
                data.add(slot.getName(),ItemStack.OPTIONAL_CODEC.encodeStart(ops,player.getEquippedStack(slot)).getOrThrow());
            data.addProperty("health",player.getHealth());
            data.addProperty("food",player.getHungerManager().getFoodLevel());
            Path temporary = playerFile.resolveSibling("steve-player.json.tmp");
            Files.writeString(temporary,new GsonBuilder().create().toJson(data));
            Files.move(temporary,playerFile,StandardCopyOption.REPLACE_EXISTING);
        } catch (Exception e) { LOG.error("Cannot save bridge player",e); }
    }
    private void grantMissing(ItemStack stack,int preferred) {
        for(int i=0;i<36;i++) if(player.getInventory().getStack(i).isOf(stack.getItem())) return;
        if(player.getInventory().getStack(preferred).isEmpty()) {player.getInventory().setStack(preferred,stack);return;}
        for(int i=9;i<36;i++) if(player.getInventory().getStack(i).isEmpty()) {player.getInventory().setStack(i,stack);return;}
    }
    private void equipDefaultArmor(MinecraftServer server) {
        var registry=server.getRegistryManager().getOrThrow(RegistryKeys.ENCHANTMENT);
        var slots=new EquipmentSlot[]{EquipmentSlot.HEAD,EquipmentSlot.CHEST,EquipmentSlot.LEGS,EquipmentSlot.FEET};
        var items=new net.minecraft.item.Item[]{Items.DIAMOND_HELMET,Items.DIAMOND_CHESTPLATE,Items.DIAMOND_LEGGINGS,Items.DIAMOND_BOOTS};
        for(int i=0;i<slots.length;i++) {
            if(!player.getEquippedStack(slots[i]).isEmpty()) continue;
            var armor=new ItemStack(items[i]);
            armor.addEnchantment(registry.getOrThrow(Enchantments.PROTECTION),4);
            armor.addEnchantment(registry.getOrThrow(Enchantments.UNBREAKING),3);
            player.equipStack(slots[i],armor);
        }
    }
    private void read(ByteBuffer p) {
        sequence = p.getLong(0); inputTime = p.getLong(8);
        x = p.getFloat(16); y = p.getFloat(20); z = p.getFloat(24);
        yaw = p.getFloat(28); pitch = p.getFloat(32);
        buttons = p.getInt(36); locked = p.getInt(40); slot = p.getInt(44);
        tx = p.getFloat(48); ty = p.getFloat(52); tz = p.getFloat(56);
        pressed |= p.getInt(60); damage += p.getFloat(64);
        int newScene = p.getInt(68);
        flags = (flags & 1) | p.getInt(72);
        targetToken=p.getInt(100); targetHealth=p.getFloat(104); targetEnemy=p.getInt(108);
        if (scene != newScene) {
            scene = newScene; originX = x; originY = y; originZ = z; abortMining();
            world.setBlockState(new BlockPos(0,65,2),Blocks.STONE.getDefaultState());
            flying=false;
            for(Cell cell:cells){if(cell.active&&cell.shadow!=null)world.removeBlock(cell.shadow,false);cell.active=false;cell.token=0;}
            miningCell=null;terrainTarget=null;
            for(Charge charge:charges) {
                if(charge.entity!=null)charge.entity.discard();
                if(charge.position!=null && world.getBlockState(charge.position).isOf(Blocks.TNT))world.setBlockState(charge.position,Blocks.AIR.getDefaultState());
                charge.entity=null;charge.position=null;charge.fuse=-2;charge.token=0;charge.unresolvedTicks=0;
            }
            tntFuse=-2;placedTnt=null;primedTnt=null;
        }
        long command=p.getLong(80);
        if(command>commandAck) {
            commandAck=command; commandResult=0;
            int operation=p.getInt(88),arg0=p.getInt(92),arg1=p.getInt(96);
            if(operation==1 && arg0>=0 && arg0<36 && arg1>=0 && arg1<36) {
                ItemStack first=player.getInventory().getStack(arg0);
                player.getInventory().setStack(arg0,player.getInventory().getStack(arg1));
                player.getInventory().setStack(arg1,first); commandResult=1;
            } else if(operation==2 && arg0>=0 && arg0<36) {
                ItemStack first=player.getInventory().getStack(arg0);
                if(first.isEmpty()||first.isOf(Items.ELYTRA)||first.isOf(Items.DIAMOND_CHESTPLATE)) {
                    player.getInventory().setStack(arg0,player.getEquippedStack(EquipmentSlot.CHEST));
                    player.equipStack(EquipmentSlot.CHEST,first); commandResult=1;
                }
            } else if(operation==3 && arg0>=0 && arg0<10) { slot=arg0; commandResult=1; }
            else if(operation==4 && player.getEquippedStack(EquipmentSlot.CHEST).isOf(Items.ELYTRA) && (flags&2)==0) {
                flying=!flying;
                flightVelocity=player.getRotationVec(1).multiply(0.6).add(0,-0.05,0); commandResult=1;
            }
        }
        if (!Float.isFinite(x+y+z+yaw+pitch+tx+ty+tz)) { buttons = 0; pressed = 0; }
    }
    private void mine() {
        BlockPos target;
        Vec3d nativeTarget=new Vec3d(tx,ty,tz);
        if((flags&4)!=0) {
            if(nativeTarget.squaredDistanceTo(new Vec3d(x,y,z))>36){abortMining();return;}
            for(Cell cell:cells)if(cell.active&&(cell.flags&1)!=0&&cell.position.squaredDistanceTo(nativeTarget)<0.01){abortMining();return;}
            Cell found=null;
            for(Cell cell:cells)if(cell.active&&cell.flags==0&&cell.position.squaredDistanceTo(nativeTarget)<0.3){found=cell;break;}
            if(found!=null) {target=found.shadow;miningCell=found;terrainTarget=null;}
            else {
                target=new BlockPos(0,75,0);
                if(terrainTarget==null||terrainTarget.squaredDistanceTo(nativeTarget)>0.01) {
                    abortMining();world.setBlockState(target,Blocks.STONE.getDefaultState());terrainTarget=nativeTarget;
                }
            }
            if(found!=null&&found.kind==13&&player.getMainHandStack().isOf(Items.DIAMOND_AXE)) {
                world.removeBlock(target,false);player.getMainHandStack().damage(1,player,EquipmentSlot.MAINHAND);
                player.getInventory().insertStack(new ItemStack(Items.OBSIDIAN));completed();return;
            }
        } else if (locked != 0) target = BlockPos.ofFloored(tx-originX+0.5, ty-originY+65, tz-originZ+0.5);
        else target = new BlockPos(0, 65, 2); // Arena block when Zelda has no actor target.
        Vec3d savedPosition=new Vec3d(player.getX(),player.getY(),player.getZ());
        if((flags&4)!=0)player.setPosition(target.getX()+0.5,target.getY(),target.getZ()-1.5);
        try {
        if (System.getenv("STEVE_IN_GAME_TEST") != null && tick % 20 == 0)
            LOG.info("Arena mining pos={} target={} distance={} grounded={} delta={}",new Vec3d(player.getX(),player.getY(),player.getZ()),target,
                    player.squaredDistanceTo(target.toCenterPos()),player.isOnGround(),world.getBlockState(target).calcBlockBreakingDelta(player,world,target));
        if (((flags&4)==0 && player.squaredDistanceTo(target.toCenterPos()) > 25) || world.getBlockState(target).isAir()) { abortMining(); return; }
        if (!target.equals(mining)) {
            mining = target;miningProgress=0;
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
        } finally {if((flags&4)!=0)player.setPosition(savedPosition);}
    }
    private void completed() {
        if(miningCell!=null) {
            for(var drop:world.getEntitiesByClass(ItemEntity.class,new Box(miningCell.shadow).expand(1),e->true)) {
                player.getInventory().insertStack(drop.getStack());if(drop.getStack().isEmpty())drop.discard();
            }
            miningCell.active=false;miningCell=null;
        } else if(terrainTarget!=null) {
            for(Cell cell:cells)if(!cell.active){cell.position=terrainTarget;cell.flags=1;cell.kind=0;cell.shadow=null;cell.token=++cellToken;cell.active=true;break;}
        }
        blockEvent++; sound(2); mining = null; miningProgress = 0; stage = -1;terrainTarget=null;
    }
    private void abortMining() {
        if (mining != null) player.interactionManager.processBlockBreakingAction(mining,
                PlayerActionC2SPacket.Action.ABORT_DESTROY_BLOCK, Direction.UP, world.getTopYInclusive(), (int)sequence);
        mining = null; miningProgress = 0; stage = -1;
        miningCell=null;terrainTarget=null;
    }
    private void placeCell(ItemStack held) {
        if((flags&8)==0)return;
        Vec3d nativePosition=new Vec3d(tx,ty,tz);
        if(nativePosition.squaredDistanceTo(new Vec3d(x,y,z))>36)return;
        for(Cell cell:cells)if(cell.active&&cell.flags==0&&cell.position.squaredDistanceTo(nativePosition)<0.3)return;
        for(int i=0;i<cells.length;i++)if(!cells[i].active) {
            Cell cell=cells[i];BlockPos shadow=new BlockPos(i%16-8,70,i/16-4);
            world.setBlockState(shadow.down(),Blocks.BEDROCK.getDefaultState());world.removeBlock(shadow,false);
            Vec3d saved=new Vec3d(player.getX(),player.getY(),player.getZ());player.setPosition(shadow.getX()+0.5,shadow.getY(),shadow.getZ()-1.5);
            int newKind=held.isOf(Items.OBSIDIAN)?13:5;
            net.minecraft.util.ActionResult result;
            try{result=player.interactionManager.interactBlock(player,world,held,Hand.MAIN_HAND,
                new BlockHitResult(shadow.down().toCenterPos(),Direction.UP,shadow.down(),false));}
            finally{player.setPosition(saved);}
            if(result.isAccepted()&&!world.getBlockState(shadow).isAir()) {
                cell.shadow=shadow;cell.position=nativePosition;cell.kind=newKind;
                cell.flags=0;cell.token=++cellToken;cell.active=true;restock();
            }
            return;
        }
    }
    private void sound(int value) { if (sounds.size() < 64) sounds.addLast(value); }
    private void useBlock(ItemStack held) {
        boolean placing=held.isOf(Items.TNT);
        Charge selected=null;
        BlockPos base;
        if(held.isOf(Items.TNT)) {
            for(Charge charge:charges)if(charge.fuse==-2||charge.fuse==0){selected=charge;break;}
            if(selected==null)return;
            Vec3d ahead=new Vec3d(player.getX(),player.getY(),player.getZ()).add(player.getRotationVec(1).multiply(2));
            base=BlockPos.ofFloored(ahead.x,64,ahead.z);
            if(!world.getBlockState(base.up()).isAir())return;
        } else {
            double nearest=25;
            for(Charge charge:charges)if(charge.fuse==-1 && charge.position!=null) {
                double distance=charge.nativePosition.squaredDistanceTo(new Vec3d(x-originX,y-originY,z-originZ));
                if(distance<nearest){selected=charge;nearest=distance;}
            }
            if(selected==null)return;
            base=selected.position;
        }
        var result=player.interactionManager.interactBlock(player,world,held,Hand.MAIN_HAND,
                new BlockHitResult(base.toCenterPos(),Direction.UP,base,false));
        if(result.isAccepted()) {
            if(placing) {
                placedTnt=base.up(); tntFuse=-1; tntEvent++; selected.position=placedTnt;selected.fuse=-1;selected.entity=null;selected.token=tntEvent;
                selected.nativePosition=new Vec3d(x-originX+placedTnt.getX()+0.5-player.getX(),y-originY,z-originZ+placedTnt.getZ()+0.5-player.getZ());
                if((flags&8)!=0)selected.nativePosition=new Vec3d(tx-originX,ty-originY-0.5,tz-originZ);
            }
            else {
                var entities=world.getEntitiesByClass(TntEntity.class,new Box(base).expand(0.75),e->true);
                if(!entities.isEmpty()) { primedTnt=entities.getFirst(); tntFuse=primedTnt.getFuse(); tntEvent++;selected.entity=primedTnt;selected.fuse=tntFuse; }
            }
        }
    }
    private void updateTnt() {
        for(Charge charge:charges) {
            if(charge.entity==null && (charge.fuse==-1 || charge.fuse==-3) && !world.getBlockState(charge.position).isOf(Blocks.TNT)) {
                var entities=world.getEntitiesByClass(TntEntity.class,new Box(charge.position).expand(0.75),e->true);
                if(!entities.isEmpty()){charge.entity=entities.getFirst();charge.fuse=charge.entity.getFuse();charge.unresolvedTicks=0;}
                else charge.fuse=++charge.unresolvedTicks>=5?0:-3;
            }
            if(charge.entity!=null) {
                if(charge.entity.isRemoved()){charge.entity=null;charge.fuse=0;}
                else charge.fuse=charge.entity.getFuse();
            }
        }
        if(primedTnt!=null) {
            if(primedTnt.isRemoved()) { primedTnt=null; tntFuse=0; tntEvent++; }
            else tntFuse=primedTnt.getFuse();
        }
    }
    private int kind(ItemStack held) {
        return held.isOf(Items.DIAMOND_PICKAXE)?1:held.isOf(Items.IRON_SWORD)?2:held.isOf(Items.GOLDEN_APPLE)?3:
            held.isOf(Items.BOW)?4:held.isOf(Items.ARROW)?6:held.isOf(Items.TNT)?7:
            held.isOf(Items.FLINT_AND_STEEL)?8:held.isOf(Items.ELYTRA)?9:held.isOf(Items.DIAMOND_AXE)?10:
            held.isOf(Items.SHIELD)?11:held.isOf(Items.DIAMOND_CHESTPLATE)?12:held.isOf(Items.OBSIDIAN)?13:
            held.isOf(Items.FIREWORK_ROCKET)?14:held.isOf(Items.TOTEM_OF_UNDYING)?15:held.isEmpty()?0:5;
    }
    private void item(ByteBuffer p, int offset, ItemStack stack) {
        p.putInt(offset, stack.isEmpty() ? 0 : Registries.ITEM.getRawId(stack.getItem()));
        p.putInt(offset + 4, stack.getDamage()|(infinite(stack)?0x80000000:0)|(stack.contains(DataComponentTypes.UNBREAKABLE)?0x40000000:0));
        p.putInt(offset + 8, stack.getCount());
        p.putInt(offset + 12, stack.getMaxDamage()>0 ? stack.getMaxDamage() - stack.getDamage() : 0);
    }
    private void publish() {
        ByteBuffer p = NativeBridge.reserve();
        if (p == null) return;
        p.order(ByteOrder.nativeOrder());
        p.putLong(0,tick); p.putLong(16,sequence);
        item(p,24,player.getMainHandStack());
        p.putInt(40,hand); p.putFloat(44,player.getHealth()); p.putFloat(48,player.handSwingProgress);
        p.putInt(52,stage); p.putInt(56,slot);
        int count = Math.min(8, sounds.size()); p.putInt(60,count);
        for(int i=0;i<count;i++) p.putInt(64+i*4,sounds.removeFirst());
        for(int i=0;i<9;i++) item(p,96+i*16,player.getInventory().getStack(i));
        p.putLong(240,blockEvent);
        p.putInt(248,Registries.BLOCK.getRawId(world.getBlockState(new BlockPos(0,65,2)).getBlock()));
        ItemStack held=player.getMainHandStack();
        p.putInt(252,1 | (kind(held)<<8));
        for(int i=0;i<36;i++) { ItemStack stack=player.getInventory().getStack(i); item(p,256+i*16,stack); p.putInt(832+i*4,kind(stack)); }
        item(p,976,player.getEquippedStack(EquipmentSlot.CHEST)); p.putInt(992,kind(player.getEquippedStack(EquipmentSlot.CHEST)));
        int armorFlags=0;
        var armorSlots=new EquipmentSlot[]{EquipmentSlot.HEAD,EquipmentSlot.CHEST,EquipmentSlot.LEGS,EquipmentSlot.FEET};
        var armorItems=new net.minecraft.item.Item[]{Items.DIAMOND_HELMET,Items.DIAMOND_CHESTPLATE,Items.DIAMOND_LEGGINGS,Items.DIAMOND_BOOTS};
        for(int i=0;i<armorSlots.length;i++) {
            var stack=player.getEquippedStack(armorSlots[i]);
            if(stack.isOf(armorItems[i])) {armorFlags|=1<<i;if(stack.hasGlint())armorFlags|=1<<(i+4);}
        }
        p.putInt(1068,armorFlags);
        p.putInt(996,commandResult); p.putLong(1000,commandAck);
        p.putLong(1008,attackEvent); p.putFloat(1016,attackDamage); p.putInt(1020,attackTarget);
        p.putLong(1024,tntEvent);
        if(placedTnt!=null) for(Charge charge:charges)if(placedTnt.equals(charge.position)) {
            p.putFloat(1032,(float)charge.nativePosition.x);p.putFloat(1036,(float)charge.nativePosition.y);p.putFloat(1040,(float)charge.nativePosition.z);break;
        }
        p.putInt(1044,tntFuse);
        p.putFloat(1048,(float)flightVelocity.x); p.putFloat(1052,(float)flightVelocity.y); p.putFloat(1056,(float)flightVelocity.z);
        p.putInt(1060,flying?1:0); p.putFloat(1064,player.getAttackCooldownProgress(0));
        for(int i=0;i<8;i++) {
            Charge charge=charges[i];int offset=1088+i*24;
            if(charge.position!=null) {p.putFloat(offset,(float)charge.nativePosition.x);p.putFloat(offset+4,(float)charge.nativePosition.y);p.putFloat(offset+8,(float)charge.nativePosition.z);}
            p.putInt(offset+12,charge.fuse);p.putLong(offset+16,charge.token);
        }
        for(int i=0;i<cells.length;i++) {
            Cell cell=cells[i];int offset=1280+i*32;
            p.putFloat(offset,(float)cell.position.x);p.putFloat(offset+4,(float)cell.position.y);p.putFloat(offset+8,(float)cell.position.z);
            p.putInt(offset+12,cell.kind);p.putLong(offset+16,cell.token);p.putInt(offset+24,cell.active?1:0);p.putInt(offset+28,cell.flags);
        }
        NativeBridge.publish();
    }
}
