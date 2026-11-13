export interface ExternalServerWorld {
  id: string;
  name: string;
}

export interface NetworkWorldDetails {
  id: string;
  type: number;
}

export interface NoAdServersPolicy {
  hideFeaturedServers: boolean;
  hideCreatorServers: boolean;
}

export interface NoAdServersRuntime {
  React: {
    createElement: (...args: any[]) => any;
    Fragment: any;
    useEffect: (fn: () => void | (() => void), deps?: any[]) => void;
  };
  Facet: {
    useSharedFacet: (facet: any) => any;
    useFacetMap: <T>(mapper: (...args: any[]) => T, deps: any[], facets: any[]) => any;
    useFacetUnwrap: <T>(facet: any) => T | symbol;
    NO_VALUE: symbol;
  };
  Facets: {
    externalServers: any;
    networkDetails: any;
    serverType: { External: number };
  };
  useHistory: () => {
    replace: (path: string) => void;
  };
  formatRouteType: (type: number) => string;
  OriginalServersTab: (props: {
    routePrefix?: string;
    showFeaturedServers?: boolean;
    showCreatorServers?: boolean;
  }) => any;
}

export function getRuntime(): NoAdServersRuntime {
  return (window as any).__NoAdServersRuntime as NoAdServersRuntime;
}

export function getPolicy(): NoAdServersPolicy {
  return (window as any).__NAS_POLICY__ as NoAdServersPolicy;
}
