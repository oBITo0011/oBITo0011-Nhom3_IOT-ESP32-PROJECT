package com.example.iot.service;

import com.example.iot.config.MqttGateway;
import com.example.iot.dto.CommandAckPayload;
import com.example.iot.entity.Command;
import com.example.iot.entity.Device;
import com.example.iot.repository.CommandRepository;
import com.example.iot.repository.DeviceRepository;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.ZonedDateTime;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;

@Slf4j
@Service
@RequiredArgsConstructor
public class CommandService {

    private final CommandRepository commandRepository;
    private final DeviceRepository deviceRepository;
    private final MqttGateway mqttGateway;

    private final ObjectMapper objectMapper = new ObjectMapper();

    /**
     * Gửi command tới ESP32 thông qua MQTT.
     *
     * Các action được hỗ trợ:
     * - LED_ON
     * - LED_OFF
     * - BUZZER_ON
     * - BUZZER_OFF
     */
    @Transactional
    public Command sendCommand(String deviceId, String action) {
        return sendCommand(deviceId, action, Map.of());
    }

    /**
     * Gửi command với dữ liệu bổ sung (ví dụ ngưỡng cảnh báo) trong payload MQTT.
     */
    @Transactional
    public Command sendCommand(
            String deviceId,
            String action,
            Map<String, Object> commandData
    ) {

        /* =====================================================
         * 1. KIỂM TRA DEVICE
         * ===================================================== */
        Device device = deviceRepository
                .findByDeviceId(deviceId)
                .orElseThrow(() ->
                        new RuntimeException("Device not found: " + deviceId)
                );

        /* =====================================================
         * 2. KIỂM TRA ACTION
         * ===================================================== */

        boolean validAction =
                "LED_ON".equals(action)
                        || "LED_OFF".equals(action)
                        || "BUZZER_ON".equals(action)
                        || "BUZZER_OFF".equals(action)
                        || "SET_THRESHOLD".equals(action);

        if (!validAction) {
            log.warn(
                    "Invalid command action received: {}",
                    action
            );

            throw new IllegalArgumentException(
                    "Invalid action: " + action
            );
        }

        /* =====================================================
         * 3. TẠO COMMAND
         * ===================================================== */

        Command command = new Command();

        command.setId(UUID.randomUUID());
        command.setDeviceId(deviceId);
        command.setAction(action);
        command.setStatus("PENDING");
        command.setCreatedAt(ZonedDateTime.now());

        String username =
                SecurityContextHolder
                        .getContext()
                        .getAuthentication() != null
                ? SecurityContextHolder
                        .getContext()
                        .getAuthentication()
                        .getName()
                : "system";

        command.setCreatedBy(username);

        try {

            /* =================================================
             * 4. TẠO MQTT PAYLOAD
             * ================================================= */

            Map<String, Object> payloadMap =
                    new HashMap<>();

            /*
             * commandId rất quan trọng.
             * ESP32 sẽ dùng commandId này để ACK.
             */
            payloadMap.put(
                    "commandId",
                    command.getId().toString()
            );

            payloadMap.put(
                    "deviceId",
                    deviceId
            );

            payloadMap.put(
                    "action",
                    action
            );

            payloadMap.put(
                    "timestamp",
                    ZonedDateTime.now().toString()
            );

            /* Settings payload is preserved with the standard command envelope. */
            payloadMap.putAll(commandData);

            String payloadJson =
                    objectMapper.writeValueAsString(
                            payloadMap
                    );

            command.setPayload(payloadJson);

            /* =================================================
             * 5. LƯU COMMAND VÀO DATABASE
             * ================================================= */

            commandRepository.save(command);

            /* =================================================
             * 6. GỬI MQTT
             * ================================================= */

            String topic =
                    "device/"
                            + deviceId
                            + "/command";

            log.info(
                    "Sending MQTT command: topic={}, payload={}",
                    topic,
                    payloadJson
            );

            mqttGateway.sendToMqtt(
                    topic,
                    1,
                    payloadJson
            );

            /* =================================================
             * 7. CẬP NHẬT TRẠNG THÁI
             * ================================================= */

            command.setStatus("SENT");
            command.setSentAt(
                    ZonedDateTime.now()
            );

            commandRepository.save(command);

            log.info(
                    "Command {} [{}] sent to device {}",
                    command.getId(),
                    action,
                    deviceId
            );

            return command;

        } catch (JsonProcessingException e) {

            command.setStatus("FAILED");
            commandRepository.save(command);

            log.error(
                    "Failed to serialize command payload",
                    e
            );

            throw new RuntimeException(
                    "Failed to serialize command payload",
                    e
            );

        } catch (Exception e) {

            command.setStatus("FAILED");
            commandRepository.save(command);

            log.error(
                    "Failed to send command via MQTT",
                    e
            );

            throw new RuntimeException(
                    "Failed to send command via MQTT",
                    e
            );
        }
    }

    /**
     * Nhận ACK từ ESP32 và cập nhật command.
     */
    @Transactional
    public void processAck(CommandAckPayload ack) {

        if (ack == null ||
                ack.getCommandId() == null) {

            log.warn(
                    "Received ACK without commandId"
            );

            return;
        }

        Optional<Command> cmdOpt =
                commandRepository.findById(
                        ack.getCommandId()
                );

        if (cmdOpt.isPresent()) {

            Command command =
                    cmdOpt.get();

            command.setStatus(
                    "FAILED".equalsIgnoreCase(ack.getStatus())
                            ? "FAILED"
                            : "ACKNOWLEDGED"
            );

            command.setAcknowledgedAt(
                    ack.getTimestamp() != null
                            ? ack.getTimestamp()
                            : ZonedDateTime.now()
            );

            commandRepository.save(command);

            /*
             * LED vẫn được đồng bộ trạng thái
             * như thiết kế ban đầu.
             */
            if (ack.getLed() != null) {

                deviceRepository
                        .findByDeviceId(
                                ack.getDeviceId()
                        )
                        .ifPresent(device -> {

                            device.setLedState(
                                    ack.getLed()
                            );

                            device.setUpdatedAt(
                                    ZonedDateTime.now()
                            );

                            deviceRepository.save(
                                    device
                            );
                        });
            }

            log.info(
                    "Command {} [{}] acknowledged",
                    ack.getCommandId(),
                    ack.getAction()
            );

        } else {

            log.warn(
                    "Received ACK for unknown command ID: {}",
                    ack.getCommandId()
            );
        }
    }

    /**
     * Lấy lịch sử command.
     */
    public Page<Command> getCommandHistory(
            String deviceId,
            Pageable pageable
    ) {

        return commandRepository
                .findByDeviceIdOrderByCreatedAtDesc(
                        deviceId,
                        pageable
                );
    }
}
