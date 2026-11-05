package com.reversepc.os;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** One process-wide queue owns native RVPC2 load/execute/commit transactions. */
public final class RuntimeExecutor {
    public static final ExecutorService INSTANCE=Executors.newSingleThreadExecutor();
    private RuntimeExecutor() {}
}
