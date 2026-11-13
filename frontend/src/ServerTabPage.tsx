import { ExternalServerWorld, getPolicy, getRuntime, NetworkWorldDetails } from "./runtime";

function unwrapOr<T>(rt: ReturnType<typeof getRuntime>, facet: any, fallback: T): T {
  const val = rt.Facet.useFacetUnwrap(facet);
  return val === rt.Facet.NO_VALUE ? fallback : (val as T);
}

export function ServerTabPage(props: { routePrefix?: string }) {
  const rt = getRuntime();
  const policy = getPolicy();
  const { useEffect } = rt.React;
  const { useSharedFacet, useFacetMap } = rt.Facet;
  const history = rt.useHistory();

  const externalFacet = useSharedFacet(rt.Facets.externalServers);
  const detailsFacet = useSharedFacet(rt.Facets.networkDetails);

  const serversFacet = useFacetMap(
    (s: { externalServerWorlds: ExternalServerWorld[] }) => s.externalServerWorlds || [],
    [],
    [externalFacet]
  );
  const detailsMapFacet = useFacetMap(
    (s: { networkDetails: NetworkWorldDetails }) => s.networkDetails,
    [],
    [detailsFacet]
  );

  const servers = unwrapOr<ExternalServerWorld[]>(rt, serversFacet, []);
  const details = unwrapOr<NetworkWorldDetails | null>(rt, detailsMapFacet, null);

  // 当隐藏精选体验和创建者体验时，若当前选中的不是外部服务器，自动选中第一台外部服务器
  useEffect(() => {
    if (!policy.hideFeaturedServers || !policy.hideCreatorServers) return;
    if (servers.length === 0) return;
    const currentId = details && details.id;
    const isExternalSelected = servers.some((s) => s.id === currentId);
    if (!isExternalSelected) {
      const prefix = (props && props.routePrefix) || "/play";
      history.replace(
        `${prefix}/servers/${servers[0].id}/${rt.formatRouteType(rt.Facets.serverType.External)}`
      );
    }
  }, [servers, details && details.id]);

  const OriginalServersTab = rt.OriginalServersTab;
  return (
    <OriginalServersTab
      routePrefix={props && props.routePrefix}
      showFeaturedServers={!policy.hideFeaturedServers}
      showCreatorServers={!policy.hideCreatorServers}
    />
  );
}
