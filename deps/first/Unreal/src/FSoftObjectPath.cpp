#include <Unreal/FSoftObjectPath.hpp>
#include <Unreal/PackageName.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>
#include <Unreal/UAssetRegistry.hpp>
#include <Unreal/UnrealInitializer.hpp>
namespace RC::Unreal
{
        FSoftObjectPath::FSoftObjectPath(const UObject* InObject)
        {
            if (InObject)
            {
                SetPath(FString(InObject->GetPathName().c_str()));
            }
        }

        // palhook: FTopLevelAssetPath is declared with engine-side (COREUOBJECT_API) members that UE4SS never
        // defined; the ones this port uses live here so they link alongside FSoftObjectPath.
        bool FTopLevelAssetPath::TrySetPath(FName InPackageName, FName InAssetName)
        {
            PackageName = InPackageName;
            AssetName = InAssetName;
            return true;
        }

        FString FTopLevelAssetPath::ToString() const
        {
            FString Out;
            ToString(Out);
            return Out;
        }

        void FTopLevelAssetPath::ToString(FString& OutString) const
        {
            OutString.Reset();
            AppendString(OutString);
        }

        void FTopLevelAssetPath::AppendString(FString& OutString) const
        {
            if (PackageName.GetComparisonIndex() == 0) return;
            OutString += PackageName.ToFString();
            if (AssetName.GetComparisonIndex() != 0)
            {
                OutString += '.';
                OutString += AssetName.ToFString();
            }
        }

        FName FSoftObjectPath::GetAssetPathName() const
        {
            if (IsNull()) return FName();
            return FName(*AssetPath.ToString(), FNAME_Add);
        }

        FString FSoftObjectPath::ToString() const
        {
            if (SubPathString.IsEmpty())
            {
                return GetAssetPathString();
            }
            auto AssetPathString = AssetPath.ToString();
            FString FullPathString;
            FullPathString.Reserve(AssetPathString.Len() + SubPathString.Len() + 1);
            FullPathString += AssetPathString;
            FullPathString += ':';
            FullPathString += SubPathString;
            return FullPathString;
        }

        // "/Package/Path.AssetName[:SubPath]" -> AssetPath{/Package/Path, AssetName}, SubPathString.
        // A path with no '.' after the last '/' is a package reference (asset name None).
        void FSoftObjectPath::SetPath(const FString& Path)
        {
            if (Path.IsEmpty() || Path == STR("None"))
            {
                Reset();
                return;
            }
            int32 ColonIndex;
            FString AssetPart;
            if (Path.FindChar(':', ColonIndex))
            {
                AssetPart = Path.Left(ColonIndex);
                SubPathString = Path.Mid(ColonIndex + 1);
            }
            else
            {
                AssetPart = Path;
                SubPathString.Empty();
            }
            int32 LastSlash = -1, LastDot = -1;
            for (int32 i = 0; i < AssetPart.Len(); ++i)
            {
                if (AssetPart[i] == '/') LastSlash = i;
                else if (AssetPart[i] == '.') LastDot = i;
            }
            if (LastDot > LastSlash)
            {
                AssetPath.TrySetPath(FName(*AssetPart.Left(LastDot), FNAME_Add), FName(*AssetPart.Mid(LastDot + 1), FNAME_Add));
            }
            else
            {
                AssetPath.TrySetPath(FName(*AssetPart, FNAME_Add), FName());
            }
        }

        UObject* FSoftObjectPath::ResolveObject() const
        {
            if (IsNull())
            {
                return nullptr;
            }
            return ResolveObjectInternal();
        }

        UObject* FSoftObjectPath::ResolveObjectInternal() const
        {
            if (SubPathString.IsEmpty())
            {
                return ResolveObjectInternal(*AssetPath.ToString());
            }
            else
            {
                return  ResolveObjectInternal(*ToString());
            }
        }

        UObject* FSoftObjectPath::ResolveObjectInternal(const TCHAR* PathString) const
        {
            return UObjectGlobals::FindObject<Unreal::UObject>(nullptr, PathString);
        }

        int32 FSoftObjectPath::GetCurrentTag()
        {
            //TODO: Find way to access tag value
            return 0;
        }

        UObject* FSoftObjectPath::TryLoad(/*FUObjectSerializeContext* InLoadContext = nullptr*/) const
        {

            // RE-UE4SS FIX (Corporalwill): [StaticLoadObject not implemented, reusing code from UE4SS lua function LoadAsset]
            //                              Perhaps we should eventually implement StaticLoadObject and just wrap this code ?
            if (!Unreal::IsInGameThread())
            {
                throw std::runtime_error{"FSoftObjectPath::TryLoad can only be called from within the game thread"};
            }

            auto asset_path_and_name = FName(*this->ToString(), EFindName::FNAME_Add);

            auto* asset_registry = static_cast<UAssetRegistry*>(UAssetRegistryHelpers::GetAssetRegistry().ObjectPointer);
            if (!asset_registry)
            {
                throw std::runtime_error{"FSoftObjectPath::TryLoad did not load assets because asset_registry was nullptr\n"};
            }

            Unreal::UObject* loaded_asset{};
            //bool was_asset_found{};
            //bool did_asset_load{};

            Unreal::FAssetData asset_data = asset_registry->GetAssetByObjectPath(asset_path_and_name);
            if ((Unreal::Version::IsAtMost(5, 0) && asset_data.ObjectPath().GetComparisonIndex()) || asset_data.PackageName().GetComparisonIndex())
            {
                loaded_asset = Unreal::UAssetRegistryHelpers::GetAsset(asset_data);
                /*if (loaded_asset)
                {
                    did_asset_load = true;
                    Output::send(STR("Asset loaded\n"));
                }
                else
                {
                    Output::send(STR("Asset was found but not loaded, could be a package\n"));
                }*/
            }
            // RE-UE4SS FIX END

            return loaded_asset;
        }

        FSoftObjectPath FSoftObjectPath::GetOrCreateIDForObject(const UObject* Object)
        {
            return FSoftObjectPath(Object);
        }
}