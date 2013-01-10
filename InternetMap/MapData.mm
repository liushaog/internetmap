//
//  MapData
//  InternetMap
//

#import "MapData.h"
#import "Node.hpp"
#import "Lines.hpp"
#import "Connection.h"
#import "IndexBox.h"

@interface MapData ()
@end

@implementation MapData

-(NodePointer)nodeAtIndex:(NSUInteger)index {
    return self.nodes.at(index);
}

-(void)loadFromFile:(NSString*)filename {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    
    NSString *fileContents = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    NSArray *lines = [fileContents componentsSeparatedByString:@"\n"];
    
    NSArray* header = [[lines objectAtIndex:0] componentsSeparatedByString:@"  "];
    int numNodes = [[header objectAtIndex:0] intValue];
    int numConnections = [[header objectAtIndex:1] intValue];
    
    self.nodes = std::vector<NodePointer>();
    self.nodesByAsn = std::map<std::string, NodePointer>();

    for (int i = 0; i < numNodes; i++) {
        NSArray* nodeDesc = [[lines objectAtIndex:1 + i] componentsSeparatedByString:@" "];
        
        Node* node = new Node();
        node->asn = std::string([[nodeDesc objectAtIndex:0] UTF8String]);
        node->index = i;
        node->importance = [[nodeDesc objectAtIndex:1] floatValue];
        node->positionX = [[nodeDesc objectAtIndex:2] floatValue];
        node->positionY = [[nodeDesc objectAtIndex:3] floatValue];
        node->type = AS_UNKNOWN;
        
        std::shared_ptr<Node> ptrNode(node);
        self.nodes.push_back(ptrNode);
        self.nodesByAsn.insert(std::make_pair(node->asn, ptrNode));
    }
    
    self.connections = [NSMutableArray new];
    
    for (int i = 0; i < numConnections; i++) {
        NSArray* connectionDesc = [[lines objectAtIndex:1 + numNodes + i] componentsSeparatedByString:@" "];
        
        Connection* connection = [Connection new];
        connection.first = self.nodesByAsn[std::string([[connectionDesc objectAtIndex:0] UTF8String])];
        connection.second = self.nodesByAsn[std::string([[connectionDesc objectAtIndex:1] UTF8String])];
        connection.first->connections.push_back(connection);
        connection.second->connections.push_back(connection);
        [self.connections addObject:connection];
    }
    
    [self createNodeBoxes];
    
    NSLog(@"load : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}

- (void)createNodeBoxes {
    
    self.boxesForNodes = [NSMutableArray array];
    
    for (int k = 0; k < numberOfCellsZ; k++) {
        float z = IndexBoxMinZ + boxSizeZWithoutOverlap*k;
        for (int j = 0; j < numberOfCellsY; j++) {
            float y = IndexBoxMinY + boxSizeYWithoutOverlap*j;
            for(int i = 0; i < numberOfCellsX; i++) {
                float x = IndexBoxMinX + boxSizeXWithoutOverlap*i;
                IndexBox* box = [[IndexBox alloc] init];
                box.center = GLKVector3Make(x+boxSizeXWithoutOverlap/2, y+boxSizeYWithoutOverlap/2, z+boxSizeZWithoutOverlap/2);
                box.minCorner = GLKVector3Make(x, y, z);
                box.maxCorner = GLKVector3Make(x+boxSizeXWithoutOverlap, y+boxSizeYWithoutOverlap, z+boxSizeZWithoutOverlap);
                [self.boxesForNodes addObject:box];
            }
        }
    }
    
    for (int i = 0; i < self.nodes.size(); i++) {
        NodePointer ptrNode = self.nodes.at(i);
        GLKVector3 pos = [self.visualization nodePosition:ptrNode];
        IndexBox* box = [self indexBoxForPoint:pos];
        [box.indices addIndex:i];
    }
}

- (IndexBox*)indexBoxForPoint:(GLKVector3)point {
    GLKVector3 pos = point;
    
    int posX = (int)fabsf((pos.x + fabsf(IndexBoxMinX))/boxSizeXWithoutOverlap);
    int posY = (int)fabsf((pos.y + fabsf(IndexBoxMinY))/boxSizeYWithoutOverlap);
    int posZ = (int)fabsf((pos.z + fabsf(IndexBoxMinZ))/boxSizeZWithoutOverlap);
    int posInArray = posX + (fabsf(IndexBoxMinX) + fabsf(IndexBoxMaxX))/boxSizeXWithoutOverlap*posY + (fabsf(IndexBoxMinX) + fabsf(IndexBoxMaxX))/boxSizeXWithoutOverlap*(fabsf(IndexBoxMinY)+fabsf(IndexBoxMaxY))/boxSizeYWithoutOverlap*posZ;
    
    return [self.boxesForNodes objectAtIndex:posInArray];

}

- (void)addNodesToBox:(IndexBox*)box {
    for (int i = 0; i < self.nodes.size(); i++) {
        NodePointer node = self.nodes.at(i);
        GLKVector3 pos = [self.visualization nodePosition:node];
        if ([box isPointInside:pos]) {
            [box.indices addIndex:i];
        }
    }
}

-(void)loadFromAttrFile:(NSString*)filename {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];

    NSDictionary *asTypeDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                [NSNumber numberWithInt:AS_UNKNOWN], @"abstained",
                                [NSNumber numberWithInt:AS_T1], @"t1",
                                [NSNumber numberWithInt:AS_T2], @"t2",
                                [NSNumber numberWithInt:AS_COMP], @"comp",
                                [NSNumber numberWithInt:AS_EDU], @"edu",
                                [NSNumber numberWithInt:AS_IX], @"ix",
                                [NSNumber numberWithInt:AS_NIC], @"nic",
                                nil];

    NSString *fileContents = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    NSArray *lines = [fileContents componentsSeparatedByString:@"\n"];

    for(NSString *line in lines) {
        NSArray* asDesc = [line componentsSeparatedByString:@"\t"];
        
        Node* node = [self.nodesByAsn objectForKey:[asDesc objectAtIndex:0]];
        if(node){
            node.type = [[asTypeDict objectForKey: [asDesc objectAtIndex:7]] intValue];
            node.typeString = [asDesc objectAtIndex:7];
            node.textDescription = [asDesc objectAtIndex:1];
        }
    }
    
    
    NSLog(@"attr load : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}


-(void)loadAsInfo:(NSString*)filename {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    
    NSString *fileContents = [NSString stringWithContentsOfFile:filename encoding:NSUTF8StringEncoding error:NULL];
    NSError *parseError = nil;
    NSData* data = [fileContents dataUsingEncoding:NSUTF8StringEncoding];
    id jsonObject = [NSJSONSerialization JSONObjectWithData:data options:0 error:&parseError];
    
//    NSLog(@"%d", [jsonObject count]);
    for(id key in jsonObject)
    {
        Node* node = [self.nodesByAsn objectForKey:key];
        if(node){
            NSArray *as = [jsonObject objectForKey:key];
            node.name = [as objectAtIndex:1];
            node.textDescription = [as objectAtIndex:5];
            node.dateRegistered = [as objectAtIndex:3];
            node.address = [as objectAtIndex:7];
            node.city = [as objectAtIndex:8];
            node.state = [as objectAtIndex:9];
            node.postalCode = [as objectAtIndex:10];
            node.country = [as objectAtIndex:11];
        }
    }

    NSLog(@"asinfo load : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}


-(void)updateDisplay:(MapDisplay*)display {
    NSTimeInterval start = [NSDate timeIntervalSinceReferenceDate];
    [self.visualization resetDisplay:display forNodes:self.nodes];
    [self.visualization updateLineDisplay:display forConnections:self.connections];
        
    NSLog(@"update display : %.2fms", ([NSDate timeIntervalSinceReferenceDate] - start) * 1000.0f);
}

@end
