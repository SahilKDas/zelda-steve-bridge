package org.ocarina.bridge.mixin;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.world.World;
import net.minecraft.world.WorldProperties;
import net.minecraft.world.level.ServerWorldProperties;
import net.minecraft.world.chunk.ChunkLoadProgress;
import net.minecraft.util.math.BlockPos;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
@Mixin(MinecraftServer.class)
public abstract class SkipSpawnMixin {
    // The bridge needs one bounded arena, not the normal 441 spawn chunks.
    @Inject(method = "prepareStartRegion", at = @At("HEAD"), cancellable = true)
    private void steve$skipSpawn(CallbackInfo ci) { ci.cancel(); }
    @Inject(method = "setupSpawn", at = @At("HEAD"), cancellable = true)
    private static void steve$fixedSpawn(ServerWorld world, ServerWorldProperties properties,
            boolean bonusChest, boolean debugWorld, ChunkLoadProgress progress, CallbackInfo ci) {
        properties.setSpawnPoint(WorldProperties.SpawnPoint.create(World.OVERWORLD,new BlockPos(0,65,0),0,0));
        properties.setInitialized(true);
        ci.cancel();
    }
}
