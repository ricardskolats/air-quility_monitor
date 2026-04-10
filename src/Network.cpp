#include "Network.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

LOG_MODULE_REGISTER(network, CONFIG_AQM_LOG_LEVEL);

cNetwork g_network;

// Semaphore to wait for LTE connection
static K_SEM_DEFINE(lte_connected_sem, 0, 1);

static void lte_event_handler(const struct lte_lc_evt *const evt)
{
    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        if (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ||
            evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING) {
            LOG_INF("Connected to LTE network");
            k_sem_give(&lte_connected_sem);
        }
        break;
    case LTE_LC_EVT_PSM_UPDATE:
        LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
                evt->psm_cfg.tau, evt->psm_cfg.active_time);
        break;
    case LTE_LC_EVT_RRC_UPDATE:
        LOG_INF("RRC mode: %s",
                evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
        break;
    default:
        break;
    }
}

int cNetwork::init(void)
{
    int err;

    LOG_INF("Initializing modem...");

    err = nrf_modem_lib_init();
    if (err) {
        LOG_ERR("Modem init failed: %d", err);
        return err;
    }

    lte_lc_register_handler(lte_event_handler);

    connected_ = false;
    LOG_INF("Modem initialized");
    return 0;
}

int cNetwork::connect(void)
{
    int err;

    if (connected_) {
        return 0;
    }

    LOG_INF("Connecting to LTE-M network...");

    // Reset semaphore
    k_sem_reset(&lte_connected_sem);

    // Start LTE connection
    err = lte_lc_connect();
    if (err) {
        LOG_ERR("LTE connect failed: %d", err);
        return err;
    }

    // Wait for connection
    err = k_sem_take(&lte_connected_sem, K_SECONDS(60));
    if (err) {
        LOG_ERR("LTE connection timeout");
        return -ETIMEDOUT;
    }

    connected_ = true;
    return 0;
}

void cNetwork::disconnect(void)
{
    if (!connected_) {
        return;
    }

    LOG_INF("Releasing network (PSM will handle sleep)");
    
    //TODO: Should be handled by PSM. Not sure, must be checked
    
    connected_ = false;
}
