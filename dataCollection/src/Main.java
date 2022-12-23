import org.json.JSONArray;
import org.json.JSONObject;

import java.io.FileWriter;
import java.io.IOException;
import java.util.HashSet;

public class Main {

    public static void main(String[] args) {
        EtherscanClient client = new EtherscanClient("IEKAXYKQ9PGC7BRXJ86I4ZX19DZ81JD2FU");
        String blockString;
        JSONObject transactionJSON;
        JSONObject outputJSON = new JSONObject();
            try {
                FileWriter fileLog = new FileWriter("log.json");
                FileWriter file = new FileWriter("data.json");
//                String blockNumber = client.getLatestBlockNumber();
                String blockNumber = "0xf6ff25";
                long hexLong = Long.parseLong(blockNumber.substring(2,blockNumber.length()), 16);
                hexLong -= 1;
                for(int i=0; i<2000; i++) {
                    System.out.println(i);
                    blockString = client.getBlockByNumber("0x"+Long.toHexString(hexLong), false);
                    fileLog.write(blockString.toString());
                    JSONObject blockJSON = new JSONObject(blockString.toString());
                    JSONObject resultForBlocks = (JSONObject) blockJSON.get("result");
                    JSONArray transactions = (JSONArray) resultForBlocks.get("transactions");
                    JSONArray states = new JSONArray();
                    for (Object s : transactions) {
                        transactionJSON = new JSONObject(client.getTransactionReceipt((String) s));
                        fileLog.write(transactionJSON.toString());
                        JSONObject resultForTransactions = (JSONObject) transactionJSON.get("result");
                        JSONArray logs = (JSONArray) resultForTransactions.get("logs");
                        for(Object log: logs){
                            JSONArray stateChanges = (JSONArray) ((JSONObject) log).get("topics");
                            HashSet stateSet = new HashSet<String>();
                            for (Object state : stateChanges) {
                                stateSet.add((String)state);
                            }
                            for(Object temp : stateSet){
                                states.put(temp);
                            }
                        }

                        Thread.sleep(205);
                    }
                    outputJSON.put("0x"+Long.toHexString(hexLong),states);
                    hexLong -= 1;
                }
                file.append(outputJSON.toString());
                file.flush();
                file.close();
                fileLog.flush();
                fileLog.close();
            } catch (IOException e) {
                FileWriter file = null;
                try {
                    file = new FileWriter("data.json");
                    file.append(outputJSON.toString());
                    file.flush();
                    file.close();
                } catch (IOException ex) {
                    throw new RuntimeException(ex);
                }
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                try {
                    FileWriter file = new FileWriter("data.json");
                    file.append(outputJSON.toString());
                    file.flush();
                    file.close();
                } catch (IOException ex) {
                    throw new RuntimeException(ex);
                }
                throw new RuntimeException(e);
            }
            finally {
                System.out.println(outputJSON);
            }

    }

}