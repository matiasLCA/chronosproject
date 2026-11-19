#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosGuardadoLibrary.generated.h"

class USaveGame;

/**
 * Guardado seguro de partidas.
 * El cargador del motor (LoadGameFromSlot) se cierra con un assert si el archivo no es un guardado
 * de Unreal (por ejemplo, texto o bytes al azar). Estas funciones leen los bytes del archivo y los
 * comprueban ANTES de dárselos al motor: si algo no cuadra, devuelven "inválido" sin cargar nada.
 */
UCLASS()
class CHRONOSGUARDADO_API UChronosGuardadoLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    /** Serializa y firma sin escribir: permite conservar la escritura atómica y las miniaturas del HUD. */
    static bool PrepararPartidaFirmada(USaveGame* Partida, TArray<uint8>& Datos);
	/** Guarda la partida en el slot con una firma al final (control de integridad). */
	UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
	static bool GuardarPartidaFirmada(USaveGame* Partida, const FString& NombreSlot, int32 IndiceUsuario = 0);

	/** Comprueba el archivo del slot sin cargarlo: cabecera de guardado, clase esperada y firma. */
	UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
	static bool ComprobarArchivoPartida(const FString& NombreSlot, TSubclassOf<USaveGame> ClaseEsperada, int32 IndiceUsuario = 0);

	/** Carga el slot solo si pasa la comprobación. Si no, devuelve None (y no carga nada). */
	UFUNCTION(BlueprintCallable, Category = "Chronos|Guardado")
	static USaveGame* CargarPartidaSegura(const FString& NombreSlot, TSubclassOf<USaveGame> ClaseEsperada, int32 IndiceUsuario = 0);
};
