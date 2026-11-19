#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosGuardadoLibrary.generated.h"

class USaveGame;

/**
 * Biblioteca de guardado seguro de The Chronos Curse.
 *
 * Valida la integridad de los archivos de partida antes de
 * entregarlos al sistema de carga de Unreal Engine.
 *
 * Permite preparar partidas firmadas en memoria, realizar
 * guardados con CRC32, verificar archivos y cargarlos
 * de forma segura.
 */
UCLASS()
class CHRONOSGUARDADO_API UChronosGuardadoLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    /**
     * Serializa y firma una partida sin escribir en disco.
     *
     * Permite integrar la escritura atomica de partidas
     * y conservar las miniaturas del sistema de guardado.
     *
     * Devuelve true si los datos se prepararon correctamente.
     */
    static bool PrepararPartidaFirmada(
        USaveGame* Partida,
        TArray<uint8>& Datos
    );

    /**
     * Guarda una partida firmada en el slot indicado.
     * Agrega una firma CRC32 para comprobar su integridad.
     */
    UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
    static bool GuardarPartidaFirmada(
        USaveGame* Partida,
        const FString& NombreSlot,
        int32 IndiceUsuario = 0
    );

    /**
     * Comprueba la integridad de un archivo de partida
     * sin cargar objetos.
     *
     * Verifica cabecera, clase esperada y firma.
     */
    UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
    static bool ComprobarArchivoPartida(
        const FString& NombreSlot,
        TSubclassOf<USaveGame> ClaseEsperada,
        int32 IndiceUsuario = 0
    );

    /**
     * Carga una partida solo si supera las comprobaciones.
     *
     * Devuelve nullptr cuando el archivo es invalido,
     * esta danado o no corresponde a la clase esperada.
     */
    UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
    static USaveGame* CargarPartidaSegura(
        const FString& NombreSlot,
        TSubclassOf<USaveGame> ClaseEsperada,
        int32 IndiceUsuario = 0
    );
};