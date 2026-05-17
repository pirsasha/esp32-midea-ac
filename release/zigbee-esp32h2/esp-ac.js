const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const ea = exposes.access;

const ATTR = {
    power: 0xF000,
    mode: 0xF001,
    fanMode: 0xF002,
    swingMode: 0xF003,
    preset: 0xF004,
    display: 0xF005,
    indoorTemp: 0xF006,
    outdoorTemp: 0xF007,
    targetTemp: 0xF008,
    firmwareVersion: 0xF009,
};

const DATA_TYPE = {
    bool: 0x10,
    uint8: 0x20,
    single: 0x39,
};

const MODE_TO_ID = {
    off: 0,
    auto: 1,
    cool: 2,
    heat: 3,
    dry: 4,
    fan_only: 5,
};

const ID_TO_MODE = ['off', 'auto', 'cool', 'heat', 'dry', 'fan_only'];

const FAN_TO_ID = {
    auto: 0,
    low: 1,
    medium: 2,
    high: 3,
    quiet: 4,
};

const ID_TO_FAN = ['auto', 'low', 'medium', 'high', 'quiet'];

const SWING_TO_ID = {
    off: 0,
    horizontal: 1,
    vertical: 2,
    both: 3,
};

const ID_TO_SWING = ['off', 'horizontal', 'vertical', 'both'];

const PRESET_TO_ID = {
    none: 0,
    sleep: 1,
    turbo: 2,
};

const ID_TO_PRESET = ['none', 'sleep', 'turbo'];

const zclSystemModeToMode = {
    0: 'off',
    1: 'auto',
    3: 'cool',
    4: 'heat',
    7: 'fan_only',
    8: 'dry',
};

const writeAttr = async (entity, cluster, attr, value, type) => {
    await entity.write(cluster, {[attr]: {value, type}});
};

const readAttrs = async (entity, cluster, attrs) => {
    await entity.read(cluster, attrs);
};

const fromZigbeeAcAnalog = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const data = msg.data;
        const result = {};

        if (data[ATTR.power] !== undefined) result.state = data[ATTR.power] ? 'ON' : 'OFF';
        if (data[ATTR.mode] !== undefined) result.system_mode = ID_TO_MODE[data[ATTR.mode]] ?? 'auto';
        if (data[ATTR.fanMode] !== undefined) result.fan_mode = ID_TO_FAN[data[ATTR.fanMode]] ?? 'auto';
        if (data[ATTR.swingMode] !== undefined) result.swing_mode = ID_TO_SWING[data[ATTR.swingMode]] ?? 'off';
        if (data[ATTR.preset] !== undefined) result.preset = ID_TO_PRESET[data[ATTR.preset]] ?? 'none';
        if (data[ATTR.display] !== undefined) result.display = data[ATTR.display] ? 'ON' : 'OFF';
        if (data[ATTR.indoorTemp] !== undefined) {
            result.local_temperature = data[ATTR.indoorTemp];
            result.indoor_temperature = data[ATTR.indoorTemp];
        }
        if (data[ATTR.outdoorTemp] !== undefined) result.outdoor_temperature = data[ATTR.outdoorTemp];
        if (data[ATTR.targetTemp] !== undefined) result.occupied_heating_setpoint = data[ATTR.targetTemp];
        if (data[ATTR.firmwareVersion] !== undefined) result.firmware_version = data[ATTR.firmwareVersion];

        return result;
    },
};

const fromZigbeeThermostat = {
    cluster: 'hvacThermostat',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const result = {};
        if (msg.data.localTemp !== undefined) result.local_temperature = msg.data.localTemp / 100;
        if (msg.data.outdoorTemp !== undefined) result.outdoor_temperature = msg.data.outdoorTemp / 100;
        if (msg.data.occupiedCoolingSetpoint !== undefined) result.occupied_heating_setpoint = msg.data.occupiedCoolingSetpoint / 100;
        if (msg.data.occupiedHeatingSetpoint !== undefined) result.occupied_heating_setpoint = msg.data.occupiedHeatingSetpoint / 100;
        if (msg.data.systemMode !== undefined) result.system_mode = zclSystemModeToMode[msg.data.systemMode] ?? 'auto';
        return result;
    },
};

const toZigbee = {
    state: {
        key: ['state'],
        convertSet: async (entity, key, value) => {
            const on = value === 'ON' || value === true;
            await writeAttr(entity, 'genAnalogInput', ATTR.power, on, DATA_TYPE.bool);
            return {state: {state: on ? 'ON' : 'OFF', system_mode: on ? 'cool' : 'off'}};
        },
    },
    system_mode: {
        key: ['system_mode'],
        convertSet: async (entity, key, value) => {
            const mode = String(value);
            if (!(mode in MODE_TO_ID)) throw new Error(`Unsupported system_mode ${mode}`);
            await writeAttr(entity, 'genAnalogInput', ATTR.mode, MODE_TO_ID[mode], DATA_TYPE.uint8);
            return {state: {system_mode: mode, state: mode === 'off' ? 'OFF' : 'ON'}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.mode]),
    },
    occupied_cooling_setpoint: {
        key: ['occupied_cooling_setpoint', 'occupied_heating_setpoint', 'current_heating_setpoint', 'current_cooling_setpoint'],
        convertSet: async (entity, key, value) => {
            const temp = Number(value);
            if (!Number.isFinite(temp) || temp < 16 || temp > 30) throw new Error('Temperature must be 16..30');
            await writeAttr(entity, 'genAnalogInput', ATTR.targetTemp, temp, DATA_TYPE.single);
            return {state: {occupied_heating_setpoint: temp}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.targetTemp]),
    },
    fan_mode: {
        key: ['fan_mode'],
        convertSet: async (entity, key, value) => {
            const mode = String(value);
            if (!(mode in FAN_TO_ID)) throw new Error(`Unsupported fan_mode ${mode}`);
            await writeAttr(entity, 'genAnalogInput', ATTR.fanMode, FAN_TO_ID[mode], DATA_TYPE.uint8);
            return {state: {fan_mode: mode}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.fanMode]),
    },
    swing_mode: {
        key: ['swing_mode'],
        convertSet: async (entity, key, value) => {
            const mode = String(value);
            if (!(mode in SWING_TO_ID)) throw new Error(`Unsupported swing_mode ${mode}`);
            await writeAttr(entity, 'genAnalogInput', ATTR.swingMode, SWING_TO_ID[mode], DATA_TYPE.uint8);
            return {state: {swing_mode: mode}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.swingMode]),
    },
    preset: {
        key: ['preset'],
        convertSet: async (entity, key, value) => {
            const preset = String(value);
            if (!(preset in PRESET_TO_ID)) throw new Error(`Unsupported preset ${preset}`);
            await writeAttr(entity, 'genAnalogInput', ATTR.preset, PRESET_TO_ID[preset], DATA_TYPE.uint8);
            return {state: {preset}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.preset]),
    },
    display: {
        key: ['display'],
        convertSet: async (entity, key, value) => {
            const enabled = value === 'ON' || value === true;
            await writeAttr(entity, 'genAnalogInput', ATTR.display, enabled, DATA_TYPE.bool);
            return {state: {display: enabled ? 'ON' : 'OFF'}};
        },
        convertGet: async (entity) => readAttrs(entity, 'genAnalogInput', [ATTR.display]),
    },
};

module.exports = {
    fingerprint: [
        {modelID: 'ZigbeeAc', manufacturerName: 'Espressif'},
        {modelID: 'GasMeter1', manufacturerName: 'Espressif'},
    ],
    model: 'ZB-MIDEA-AC',
    vendor: 'Custom',
    description: 'Zigbee air conditioner Royal Clima/Midea on ESP32-H2',
    fromZigbee: [fromZigbeeAcAnalog, fromZigbeeThermostat],
    toZigbee: [
        toZigbee.state,
        toZigbee.system_mode,
        toZigbee.occupied_cooling_setpoint,
        toZigbee.fan_mode,
        toZigbee.swing_mode,
        toZigbee.preset,
        toZigbee.display,
    ],
    exposes: [
        e.climate()
            .withSetpoint('occupied_heating_setpoint', 16, 30, 0.5, ea.STATE_SET)
            .withLocalTemperature()
            .withSystemMode(['off', 'auto', 'cool', 'heat', 'dry', 'fan_only'], ea.STATE_SET),
        e.enum('fan_mode', ea.STATE_SET, ['auto', 'low', 'medium', 'high', 'quiet']).withDescription('Fan speed'),
        e.enum('swing_mode', ea.STATE_SET, ['off', 'horizontal', 'vertical', 'both']).withDescription('Swing mode'),
        e.enum('preset', ea.STATE_SET, ['none', 'sleep', 'turbo']).withDescription('Preset mode'),
        e.binary('display', ea.STATE_SET, 'ON', 'OFF').withDescription('AC display and beep control'),
        e.numeric('outdoor_temperature', ea.STATE).withUnit('C').withDescription('Outdoor unit temperature'),
        e.numeric('firmware_version', ea.STATE).withDescription('Firmware version'),
    ],
    configure: async () => {},
};
