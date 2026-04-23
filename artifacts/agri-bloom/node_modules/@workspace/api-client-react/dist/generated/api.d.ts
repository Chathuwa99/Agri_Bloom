import type { QueryKey, UseMutationOptions, UseMutationResult, UseQueryOptions, UseQueryResult } from '@tanstack/react-query';
import type { Alert, CommandResult, HealthStatus, PlantHealth, SensorData, SensorHistoryEntry, Settings, WaterCommand } from './api.schemas';
import { customFetch } from '../custom-fetch';
import type { ErrorType, BodyType } from '../custom-fetch';
type AwaitedInput<T> = PromiseLike<T> | T;
type Awaited<O> = O extends AwaitedInput<infer T> ? T : never;
type SecondParameter<T extends (...args: never) => unknown> = Parameters<T>[1];
/**
 * @summary Health check
 */
export declare const getHealthCheckUrl: () => string;
export declare const healthCheck: (options?: RequestInit) => Promise<HealthStatus>;
export declare const getHealthCheckQueryKey: () => readonly ["/api/healthz"];
export declare const getHealthCheckQueryOptions: <TData = Awaited<ReturnType<typeof healthCheck>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof healthCheck>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof healthCheck>>, TError, TData> & {
    queryKey: QueryKey;
};
export type HealthCheckQueryResult = NonNullable<Awaited<ReturnType<typeof healthCheck>>>;
export type HealthCheckQueryError = ErrorType<unknown>;
/**
 * @summary Health check
 */
export declare function useHealthCheck<TData = Awaited<ReturnType<typeof healthCheck>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof healthCheck>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Get current sensor readings
 */
export declare const getGetSensorDataUrl: () => string;
export declare const getSensorData: (options?: RequestInit) => Promise<SensorData>;
export declare const getGetSensorDataQueryKey: () => readonly ["/api/sensor/current"];
export declare const getGetSensorDataQueryOptions: <TData = Awaited<ReturnType<typeof getSensorData>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSensorData>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof getSensorData>>, TError, TData> & {
    queryKey: QueryKey;
};
export type GetSensorDataQueryResult = NonNullable<Awaited<ReturnType<typeof getSensorData>>>;
export type GetSensorDataQueryError = ErrorType<unknown>;
/**
 * @summary Get current sensor readings
 */
export declare function useGetSensorData<TData = Awaited<ReturnType<typeof getSensorData>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSensorData>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Get plant health status and score
 */
export declare const getGetPlantHealthUrl: () => string;
export declare const getPlantHealth: (options?: RequestInit) => Promise<PlantHealth>;
export declare const getGetPlantHealthQueryKey: () => readonly ["/api/sensor/health"];
export declare const getGetPlantHealthQueryOptions: <TData = Awaited<ReturnType<typeof getPlantHealth>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getPlantHealth>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof getPlantHealth>>, TError, TData> & {
    queryKey: QueryKey;
};
export type GetPlantHealthQueryResult = NonNullable<Awaited<ReturnType<typeof getPlantHealth>>>;
export type GetPlantHealthQueryError = ErrorType<unknown>;
/**
 * @summary Get plant health status and score
 */
export declare function useGetPlantHealth<TData = Awaited<ReturnType<typeof getPlantHealth>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getPlantHealth>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Get 7-day sensor history
 */
export declare const getGetSensorHistoryUrl: () => string;
export declare const getSensorHistory: (options?: RequestInit) => Promise<SensorHistoryEntry[]>;
export declare const getGetSensorHistoryQueryKey: () => readonly ["/api/sensor/history"];
export declare const getGetSensorHistoryQueryOptions: <TData = Awaited<ReturnType<typeof getSensorHistory>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSensorHistory>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof getSensorHistory>>, TError, TData> & {
    queryKey: QueryKey;
};
export type GetSensorHistoryQueryResult = NonNullable<Awaited<ReturnType<typeof getSensorHistory>>>;
export type GetSensorHistoryQueryError = ErrorType<unknown>;
/**
 * @summary Get 7-day sensor history
 */
export declare function useGetSensorHistory<TData = Awaited<ReturnType<typeof getSensorHistory>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSensorHistory>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Get all alerts
 */
export declare const getGetAlertsUrl: () => string;
export declare const getAlerts: (options?: RequestInit) => Promise<Alert[]>;
export declare const getGetAlertsQueryKey: () => readonly ["/api/alerts"];
export declare const getGetAlertsQueryOptions: <TData = Awaited<ReturnType<typeof getAlerts>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getAlerts>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof getAlerts>>, TError, TData> & {
    queryKey: QueryKey;
};
export type GetAlertsQueryResult = NonNullable<Awaited<ReturnType<typeof getAlerts>>>;
export type GetAlertsQueryError = ErrorType<unknown>;
/**
 * @summary Get all alerts
 */
export declare function useGetAlerts<TData = Awaited<ReturnType<typeof getAlerts>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getAlerts>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Acknowledge an alert
 */
export declare const getAcknowledgeAlertUrl: (id: number) => string;
export declare const acknowledgeAlert: (id: number, options?: RequestInit) => Promise<Alert>;
export declare const getAcknowledgeAlertMutationOptions: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof acknowledgeAlert>>, TError, {
        id: number;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationOptions<Awaited<ReturnType<typeof acknowledgeAlert>>, TError, {
    id: number;
}, TContext>;
export type AcknowledgeAlertMutationResult = NonNullable<Awaited<ReturnType<typeof acknowledgeAlert>>>;
export type AcknowledgeAlertMutationError = ErrorType<unknown>;
/**
* @summary Acknowledge an alert
*/
export declare const useAcknowledgeAlert: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof acknowledgeAlert>>, TError, {
        id: number;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationResult<Awaited<ReturnType<typeof acknowledgeAlert>>, TError, {
    id: number;
}, TContext>;
/**
 * @summary Trigger watering pump
 */
export declare const getTriggerWateringUrl: () => string;
export declare const triggerWatering: (waterCommand: WaterCommand, options?: RequestInit) => Promise<CommandResult>;
export declare const getTriggerWateringMutationOptions: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof triggerWatering>>, TError, {
        data: BodyType<WaterCommand>;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationOptions<Awaited<ReturnType<typeof triggerWatering>>, TError, {
    data: BodyType<WaterCommand>;
}, TContext>;
export type TriggerWateringMutationResult = NonNullable<Awaited<ReturnType<typeof triggerWatering>>>;
export type TriggerWateringMutationBody = BodyType<WaterCommand>;
export type TriggerWateringMutationError = ErrorType<unknown>;
/**
* @summary Trigger watering pump
*/
export declare const useTriggerWatering: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof triggerWatering>>, TError, {
        data: BodyType<WaterCommand>;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationResult<Awaited<ReturnType<typeof triggerWatering>>, TError, {
    data: BodyType<WaterCommand>;
}, TContext>;
/**
 * @summary Get system settings and thresholds
 */
export declare const getGetSettingsUrl: () => string;
export declare const getSettings: (options?: RequestInit) => Promise<Settings>;
export declare const getGetSettingsQueryKey: () => readonly ["/api/settings"];
export declare const getGetSettingsQueryOptions: <TData = Awaited<ReturnType<typeof getSettings>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSettings>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}) => UseQueryOptions<Awaited<ReturnType<typeof getSettings>>, TError, TData> & {
    queryKey: QueryKey;
};
export type GetSettingsQueryResult = NonNullable<Awaited<ReturnType<typeof getSettings>>>;
export type GetSettingsQueryError = ErrorType<unknown>;
/**
 * @summary Get system settings and thresholds
 */
export declare function useGetSettings<TData = Awaited<ReturnType<typeof getSettings>>, TError = ErrorType<unknown>>(options?: {
    query?: UseQueryOptions<Awaited<ReturnType<typeof getSettings>>, TError, TData>;
    request?: SecondParameter<typeof customFetch>;
}): UseQueryResult<TData, TError> & {
    queryKey: QueryKey;
};
/**
 * @summary Update system settings
 */
export declare const getUpdateSettingsUrl: () => string;
export declare const updateSettings: (settings: Settings, options?: RequestInit) => Promise<Settings>;
export declare const getUpdateSettingsMutationOptions: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof updateSettings>>, TError, {
        data: BodyType<Settings>;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationOptions<Awaited<ReturnType<typeof updateSettings>>, TError, {
    data: BodyType<Settings>;
}, TContext>;
export type UpdateSettingsMutationResult = NonNullable<Awaited<ReturnType<typeof updateSettings>>>;
export type UpdateSettingsMutationBody = BodyType<Settings>;
export type UpdateSettingsMutationError = ErrorType<unknown>;
/**
* @summary Update system settings
*/
export declare const useUpdateSettings: <TError = ErrorType<unknown>, TContext = unknown>(options?: {
    mutation?: UseMutationOptions<Awaited<ReturnType<typeof updateSettings>>, TError, {
        data: BodyType<Settings>;
    }, TContext>;
    request?: SecondParameter<typeof customFetch>;
}) => UseMutationResult<Awaited<ReturnType<typeof updateSettings>>, TError, {
    data: BodyType<Settings>;
}, TContext>;
export {};
//# sourceMappingURL=api.d.ts.map