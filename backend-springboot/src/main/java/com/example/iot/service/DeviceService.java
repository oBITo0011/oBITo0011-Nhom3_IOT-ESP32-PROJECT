package com.example.iot.service;

import com.example.iot.dto.StatusPayload;
import com.example.iot.entity.Device;
import com.example.iot.repository.DeviceRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.scheduling.annotation.Scheduled;

import java.time.Duration;
import java.time.ZonedDateTime;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Slf4j
@Service
@RequiredArgsConstructor
public class DeviceService {

    /* ESP32 publishes telemetry every 3 seconds. Five missed samples means offline. */
    private static final Duration OFFLINE_TIMEOUT = Duration.ofSeconds(15);

    private final DeviceRepository deviceRepository;

    @Transactional
    public void processStatus(StatusPayload payload) {
        if (payload.getDeviceId() == null) return;
        
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(payload.getDeviceId());
        if (deviceOpt.isPresent()) {
            Device device = deviceOpt.get();
            device.setStatus(payload.getStatus());
            device.setLastSeenAt(ZonedDateTime.now());
            device.setUpdatedAt(ZonedDateTime.now());
            deviceRepository.save(device);
            log.info("Device {} status updated to {}", device.getDeviceId(), device.getStatus());
        } else {
            // Auto register device if needed, or just log
            log.warn("Status received for unknown device: {}", payload.getDeviceId());
            Device device = new Device();
            device.setId(UUID.randomUUID());
            device.setDeviceId(payload.getDeviceId());
            device.setName("Auto-registered " + payload.getDeviceId());
            device.setType("UNKNOWN");
            device.setStatus(payload.getStatus());
            device.setLedState(false);
            device.setLastSeenAt(ZonedDateTime.now());
            device.setCreatedAt(ZonedDateTime.now());
            device.setUpdatedAt(ZonedDateTime.now());
            deviceRepository.save(device);
        }
    }

    public List<Device> getAllDevices() {
        return deviceRepository.findAll();
    }

    public Device getDeviceByDeviceId(String deviceId) {
        Device device = deviceRepository.findByDeviceId(deviceId)
                .orElseThrow(() -> new RuntimeException("Device not found"));

        markOfflineIfStale(device, ZonedDateTime.now());
        return device;
    }

    /**
     * Fallback for an unexpected power loss or missed MQTT Last Will message.
     * The database status is made OFFLINE after five missed 3-second telemetry cycles.
     */
    @Scheduled(fixedDelay = 5000)
    @Transactional
    public void markStaleDevicesOffline() {
        ZonedDateTime now = ZonedDateTime.now();
        deviceRepository.findAll().forEach(device -> markOfflineIfStale(device, now));
    }

    private void markOfflineIfStale(Device device, ZonedDateTime now) {
        if (!"ONLINE".equalsIgnoreCase(device.getStatus()) || device.getLastSeenAt() == null) {
            return;
        }

        if (Duration.between(device.getLastSeenAt(), now).compareTo(OFFLINE_TIMEOUT) > 0) {
            device.setStatus("OFFLINE");
            device.setUpdatedAt(now);
            deviceRepository.save(device);
            log.warn("Device {} marked OFFLINE after {} seconds without telemetry/status", device.getDeviceId(), OFFLINE_TIMEOUT.toSeconds());
        }
    }
}
