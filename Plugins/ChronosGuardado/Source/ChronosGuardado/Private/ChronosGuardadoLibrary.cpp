
#include "ChronosGuardadoLibrary.h"

#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

namespace ChronosGuardado
{
	// Pie agregado al final de cada partida:
	// firma "CHRN" + CRC32 del cuerpo + tamaño del cuerpo.
	static constexpr uint32 Firma = 0x4E524843;
	static constexpr int32 TamPie = 12;

	// Etiqueta inicial de los guardados modernos de Unreal: "SAVG".
	static constexpr int32 EtiquetaSAVG = 0x53415647;

	// Límite utilizado para evitar cadenas inválidas o excesivas.
	static constexpr int32 MaxTexto = 1024;

	/** Lector de bytes que evita lecturas fuera del archivo. */
	struct FLector
	{
		const uint8* Datos = nullptr;
		int32 Pos = 0;
		int32 Fin = 0;

		bool Int32(int32& Out)
		{
			if (Pos < 0 || Fin - Pos < 4)
			{
				return false;
			}

			FMemory::Memcpy(&Out, Datos + Pos, 4);
			Pos += 4;
			return true;
		}

		bool Saltar(int64 N)
		{
			if (N < 0 || N > (int64)(Fin - Pos))
			{
				return false;
			}

			Pos += (int32)N;
			return true;
		}

		bool Texto(FString& Out)
		{
			int32 Len = 0;

			if (!Int32(Len))
			{
				return false;
			}

			if (Len == 0)
			{
				Out.Reset();
				return true;
			}

			if (Len > 0)
			{
				if (Len > MaxTexto || Len > Fin - Pos)
				{
					return false;
				}

				const ANSICHAR* Ptr =
					reinterpret_cast<const ANSICHAR*>(Datos + Pos);

				if (Ptr[Len - 1] != 0)
				{
					return false;
				}

				Out = FString::ConstructFromPtrSize(Ptr, Len - 1);
				Pos += Len;
				return true;
			}

			if (Len == MIN_int32)
			{
				return false;
			}

			const int32 N = -Len;

			if (N > MaxTexto || (int64)N * 2 > (int64)(Fin - Pos))
			{
				return false;
			}

			TArray<UTF16CHAR> Buf;
			Buf.SetNumUninitialized(N);

			FMemory::Memcpy(
				Buf.GetData(),
				Datos + Pos,
				(SIZE_T)N * 2
			);

			if (Buf[N - 1] != 0)
			{
				return false;
			}

			Out = FString::ConstructFromPtrSize(
				Buf.GetData(),
				N - 1
			);

			Pos += N * 2;
			return true;
		}
	};

	/**
	 * Lee la cabecera generada por SaveGameToMemory.
	 * Recupera el nombre de la clase de guardado.
	 */
	static bool LeerCabecera(
		const uint8* Datos,
		int32 Tam,
		FString& ClaseGuardada
	)
	{
		FLector L;
		L.Datos = Datos;
		L.Fin = Tam;

		int32 Etiqueta = 0;
		int32 Version = 0;

		if (!L.Int32(Etiqueta) || Etiqueta != EtiquetaSAVG)
		{
			return false;
		}

		if (!L.Int32(Version) || Version < 1 || Version > 3)
		{
			return false;
		}

		// Versión del paquete.
		if (!L.Saltar(Version >= 3 ? 8 : 4))
		{
			return false;
		}

		// Información de versión del motor y rama.
		FString Rama;

		if (!L.Saltar(2 + 2 + 2 + 4) || !L.Texto(Rama))
		{
			return false;
		}

		if (Version >= 2)
		{
			int32 Formato = 0;
			int32 Cantidad = 0;

			if (!L.Int32(Formato) || Formato != 3)
			{
				return false;
			}

			if (!L.Int32(Cantidad) ||
				Cantidad < 0 ||
				Cantidad > 4096 ||
				!L.Saltar((int64)Cantidad * 20))
			{
				return false;
			}
		}

		return L.Texto(ClaseGuardada) &&
			   !ClaseGuardada.IsEmpty();
	}

	/**
	 * Comprueba la integridad de un archivo de guardado.
	 * Valida firma, CRC32 y clase antes de cargarlo.
	 */
	static bool ComprobarDatos(
		const FString& NombreSlot,
		int32 IndiceUsuario,
		TSubclassOf<USaveGame> ClaseEsperada,
		TArray<uint8>& Cuerpo
	)
	{
		Cuerpo.Reset();

		TArray<uint8> Datos;

		if (NombreSlot.IsEmpty() ||
			!UGameplayStatics::LoadDataFromSlot(
				Datos,
				NombreSlot,
				IndiceUsuario
			) ||
			Datos.Num() < 8)
		{
			return false;
		}

		int32 TamCuerpo = Datos.Num();

		if (Datos.Num() >= TamPie + 8)
		{
			uint32 FirmaLeida = 0;
			uint32 CrcLeido = 0;
			int32 TamLeido = 0;

			const uint8* Pie =
				Datos.GetData() + Datos.Num() - TamPie;

			FMemory::Memcpy(&FirmaLeida, Pie, 4);
			FMemory::Memcpy(&CrcLeido, Pie + 4, 4);
			FMemory::Memcpy(&TamLeido, Pie + 8, 4);

			if (FirmaLeida == Firma)
			{
				if (TamLeido != Datos.Num() - TamPie ||
					FCrc::MemCrc32(
						Datos.GetData(),
						TamLeido
					) != CrcLeido)
				{
					return false;
				}

				TamCuerpo = TamLeido;
			}
		}

		FString ClaseGuardada;

		if (!LeerCabecera(
			Datos.GetData(),
			TamCuerpo,
			ClaseGuardada
		))
		{
			return false;
		}

		if (ClaseEsperada)
		{
			if (ClaseGuardada != ClaseEsperada->GetPathName())
			{
				return false;
			}
		}
		else
		{
			UClass* Clase =
				FindObject<UClass>(
					nullptr,
					*ClaseGuardada
				);

			if (!Clase ||
				!Clase->IsChildOf(USaveGame::StaticClass()))
			{
				return false;
			}
		}

		Cuerpo.Append(Datos.GetData(), TamCuerpo);
		return true;
	}
}

/**
 * Prepara una partida firmada en memoria.
 *
 * Conserva la nueva funcionalidad de recuperar-diseno
 * sin eliminar el guardado tradicional de main.
 */
bool UChronosGuardadoLibrary::PrepararPartidaFirmada(
	USaveGame* Partida,
	TArray<uint8>& Datos
)
{
	Datos.Reset();

	if (!Partida ||
		!UGameplayStatics::SaveGameToMemory(Partida, Datos) ||
		Datos.Num() == 0)
	{
		return false;
	}

	const uint32 Crc =
		FCrc::MemCrc32(Datos.GetData(), Datos.Num());

	const int32 Tam = Datos.Num();
	const uint32 Firma = ChronosGuardado::Firma;

	Datos.Append(
		reinterpret_cast<const uint8*>(&Firma),
		4
	);

	Datos.Append(
		reinterpret_cast<const uint8*>(&Crc),
		4
	);

	Datos.Append(
		reinterpret_cast<const uint8*>(&Tam),
		4
	);

	return true;
}

/**
 * Guarda una partida firmada.
 *
 * Reutiliza PrepararPartidaFirmada para evitar
 * duplicar la lógica de firma y CRC32.
 */
bool UChronosGuardadoLibrary::GuardarPartidaFirmada(
	USaveGame* Partida,
	const FString& NombreSlot,
	int32 IndiceUsuario
)
{
	if (NombreSlot.IsEmpty())
	{
		return false;
	}

	TArray<uint8> Datos;

	if (!PrepararPartidaFirmada(Partida, Datos))
	{
		return false;
	}

	return UGameplayStatics::SaveDataToSlot(
		Datos,
		NombreSlot,
		IndiceUsuario
	);
}

/**
 * Valida un archivo de guardado sin cargar objetos.
 */
bool UChronosGuardadoLibrary::ComprobarArchivoPartida(
	const FString& NombreSlot,
	TSubclassOf<USaveGame> ClaseEsperada,
	int32 IndiceUsuario
)
{
	TArray<uint8> Cuerpo;

	return ChronosGuardado::ComprobarDatos(
		NombreSlot,
		IndiceUsuario,
		ClaseEsperada,
		Cuerpo
	);
}

/**
 * Carga una partida después de comprobar
 * su integridad y su clase.
 */
USaveGame* UChronosGuardadoLibrary::CargarPartidaSegura(
	const FString& NombreSlot,
	TSubclassOf<USaveGame> ClaseEsperada,
	int32 IndiceUsuario
)
{
	TArray<uint8> Cuerpo;

	if (!ChronosGuardado::ComprobarDatos(
		NombreSlot,
		IndiceUsuario,
		ClaseEsperada,
		Cuerpo
	))
	{
		return nullptr;
	}

	USaveGame* Partida =
		UGameplayStatics::LoadGameFromMemory(Cuerpo);

	if (Partida &&
		ClaseEsperada &&
		!Partida->IsA(ClaseEsperada))
	{
		return nullptr;
	}

	return Partida;
}
